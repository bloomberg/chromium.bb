// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for CustomTabsConnection. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class CustomTabsConnectionTest {
    private CustomTabsConnection mCustomTabsConnection;
    private static final String URL = "http://www.google.com";
    private static final String URL2 = "https://www.android.com";
    private static final String INVALID_SCHEME_URL = "intent://www.google.com";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";

    @Before
    public void setUp() throws Exception {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        mCustomTabsConnection = CustomTabsTestUtils.setUpConnection();
    }

    @After
    public void tearDown() throws Exception {
        CustomTabsTestUtils.cleanupSessions(mCustomTabsConnection);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                WarmupManager.getInstance().destroySpareWebContents();
            }
        });
    }

    /**
     * Tests that we can create a new session. Registering with a null callback
     * fails, as well as multiple sessions with the same callback.
     */
    @Test
    @SmallTest
    public void testNewSession() {
        Assert.assertEquals(false, mCustomTabsConnection.newSession(null));
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertEquals(true, mCustomTabsConnection.newSession(token));
        Assert.assertEquals(false, mCustomTabsConnection.newSession(token));
    }

    /**
     * Tests that we can create several sessions.
     */
    @Test
    @SmallTest
    public void testSeveralSessions() {
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertEquals(true, mCustomTabsConnection.newSession(token));
        CustomTabsSessionToken token2 = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertEquals(true, mCustomTabsConnection.newSession(token2));
    }

    /**
     * Tests that {@link CustomTabsConnection#warmup(long)} succeeds and can
     * be issued multiple times.
     */
    @Test
    @SmallTest
    public void testCanWarmup() throws InterruptedException, TimeoutException {
        CustomTabsTestUtils.warmUpAndWait();
        CustomTabsTestUtils.warmUpAndWait();
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCreateSpareRenderer() throws InterruptedException, TimeoutException {
        CustomTabsTestUtils.warmUpAndWait();
        // On UI thread because:
        // 1. takeSpareWebContents needs to be called from the UI thread.
        // 2. warmup() is non-blocking and posts tasks to the UI thread, it ensures proper ordering.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                WarmupManager warmupManager = WarmupManager.getInstance();
                Assert.assertTrue(warmupManager.hasSpareWebContents());
                WebContents webContents = warmupManager.takeSpareWebContents(false, false);
                Assert.assertNotNull(webContents);
                Assert.assertFalse(warmupManager.hasSpareWebContents());
                webContents.destroy();
            }
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCreateSpareRendererCanBeRecreated()
            throws InterruptedException, TimeoutException {
        CustomTabsTestUtils.warmUpAndWait();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
                Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
            }
        });
        CustomTabsTestUtils.warmUpAndWait();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
            }
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPrerenderDestroysSpareRenderer() throws InterruptedException, TimeoutException {
        CustomTabsConnection.getInstance().setForcePrerender(true);
        final CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                String referrer =
                        mCustomTabsConnection.getReferrerForSession(token).getUrl();
                WebContents webContents =
                        mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer);
                Assert.assertNotNull(webContents);
                webContents.destroy();
                Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
            }
        });
    }

    /*
     * Tests that when the disconnection notification comes from a non-UI thread, Chrome doesn't
     * crash. Non-regression test for crbug.com/623128.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPrerenderAndDisconnectOnOtherThread()
            throws InterruptedException, TimeoutException {
        final CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        final Thread otherThread = new Thread(new Runnable() {
            @Override
            public void run() {
                mCustomTabsConnection.cleanUpSession(token);
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                otherThread.start();
            }
        });
        // Should not crash, hence no assertions below.
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMayLaunchUrlKeepsSpareRendererWithoutPrerendering()
            throws InterruptedException, TimeoutException {
        CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));

        Bundle extras = new Bundle();
        extras.putInt(
                CustomTabsConnection.DEBUG_OVERRIDE_KEY, CustomTabsConnection.NO_PRERENDERING);
        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), extras, null));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
            }
        });
    }

    @Test
    @SmallTest
    public void testMayLaunchUrlNullOrEmptyUrl() throws InterruptedException, TimeoutException {
        assertWarmupAndMayLaunchUrl(null, null, true);
        CustomTabsTestUtils.cleanupSessions(mCustomTabsConnection); // Resets throttling.
        assertWarmupAndMayLaunchUrl(null, "", true);
    }

    /**
     * Tests that a new mayLaunchUrl() call destroys the previous hidden tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB})
    @RetryOnFailure
    public void testOnlyOneHiddenTab() throws Exception {
        Assert.assertTrue("Failed warmup()", mCustomTabsConnection.warmup(0));
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue("Failed newSession()", mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setSpeculationModeForSession(
                token, CustomTabsConnection.SpeculationParams.HIDDEN_TAB);

        // First hidden tab, add an observer to check that it's destroyed.
        Assert.assertTrue("Failed first mayLaunchUrl()",
                mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        final Semaphore destroyed = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertNotNull(
                        "Null speculation, first one", mCustomTabsConnection.mSpeculation);
                Tab tab = mCustomTabsConnection.mSpeculation.tab;
                Assert.assertNotNull("No first tab", tab);
                tab.addObserver(new EmptyTabObserver() {
                    @Override
                    public void onDestroyed(Tab destroyedTab) {
                        destroyed.release();
                    }
                });
            }
        });

        // New hidden tab.
        mCustomTabsConnection.resetThrottling(Process.myUid());
        Assert.assertTrue("Failed second mayLaunchUrl()",
                mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL2), null, null));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertNotNull(
                        "Null speculation, new hidden tab", mCustomTabsConnection.mSpeculation);
                Assert.assertNotNull("No second tab", mCustomTabsConnection.mSpeculation.tab);
                Assert.assertEquals(URL2, mCustomTabsConnection.mSpeculation.url);
            }
        });

        Assert.assertTrue(
                "Only one hidden tab should exist.", destroyed.tryAcquire(10, TimeUnit.SECONDS));

        // Clears the second hidden tab.
        mCustomTabsConnection.resetThrottling(Process.myUid());
        Assert.assertTrue("Failed cleanup mayLaunchUrl()",
                mCustomTabsConnection.mayLaunchUrl(token, null, null, null));
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testUnderstandsLowConfidenceMayLaunchUrl() {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        List<Bundle> urls = new ArrayList<>();
        Bundle urlBundle = new Bundle();
        urlBundle.putParcelable(CustomTabsService.KEY_URL, Uri.parse(URL));
        urls.add(urlBundle);
        mCustomTabsConnection.mayLaunchUrl(token, null, null, urls);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                Assert.assertNull(mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer));
            }
        });
    }

    @Test
    @SmallTest
    public void testLowConfidenceMayLaunchUrlOnlyAcceptUris()
            throws InterruptedException, TimeoutException {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        CustomTabsTestUtils.warmUpAndWait();

        final List<Bundle> urlsAsString = new ArrayList<>();
        Bundle urlStringBundle = new Bundle();
        urlStringBundle.putString(CustomTabsService.KEY_URL, URL);
        urlsAsString.add(urlStringBundle);

        final List<Bundle> urlsAsUri = new ArrayList<>();
        Bundle urlUriBundle = new Bundle();
        urlUriBundle.putParcelable(CustomTabsService.KEY_URL, Uri.parse(URL));
        urlsAsUri.add(urlUriBundle);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse(mCustomTabsConnection.lowConfidenceMayLaunchUrl(urlsAsString));
                Assert.assertTrue(mCustomTabsConnection.lowConfidenceMayLaunchUrl(urlsAsUri));
            }
        });
    }

    @Test
    @SmallTest
    public void testLowConfidenceMayLaunchUrlDoesntCrash()
            throws InterruptedException, TimeoutException {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        CustomTabsTestUtils.warmUpAndWait();

        final List<Bundle> invalidBundles = new ArrayList<>();
        Bundle invalidBundle = new Bundle();
        invalidBundle.putParcelable(CustomTabsService.KEY_URL, new Intent());
        invalidBundles.add(invalidBundle);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mCustomTabsConnection.lowConfidenceMayLaunchUrl(invalidBundles);
            }
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testStillHighConfidenceMayLaunchUrlWithSeveralUrls() {
        CustomTabsConnection.getInstance().setForcePrerender(true);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        List<Bundle> urls = new ArrayList<>();
        Bundle urlBundle = new Bundle();
        urlBundle.putParcelable(CustomTabsService.KEY_URL, Uri.parse(URL));
        urls.add(urlBundle);

        mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, urls);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertNull(WarmupManager.getInstance().takeSpareWebContents(false, false));
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                Assert.assertNotNull(
                        mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer));
            }
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPrefetchOnlyNoPrerenderHasSpareWebContents()
            throws InterruptedException, TimeoutException {
        CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));

        Bundle extras = new Bundle();
        extras.putInt(CustomTabsConnection.DEBUG_OVERRIDE_KEY, CustomTabsConnection.PREFETCH_ONLY);
        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), extras, null));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
            }
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testCanCancelPrerender() throws InterruptedException, TimeoutException {
        CustomTabsConnection.getInstance().setForcePrerender(true);
        final CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(URL, mCustomTabsConnection.getSpeculatedUrl(token));
            }
        });

        // Two mayLaunchUrl() can be called so close to each other they get throttled.
        CustomTabsConnection.getInstance().resetThrottling(Process.myUid());

        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, null, null, null));
        // mayLaunchUrl() posts a task, the following has to run after it.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                Assert.assertNull(mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer));
            }
        });
    }

    private void assertSpareWebContentsNotNullAndDestroy() {
        WebContents webContents = WarmupManager.getInstance().takeSpareWebContents(false, false);
        Assert.assertNotNull(webContents);
        webContents.destroy();
    }

    /**
     * Calls warmup() and mayLaunchUrl(), checks for the expected result
     * (success or failure) and returns the result code.
     */
    private CustomTabsSessionToken assertWarmupAndMayLaunchUrl(CustomTabsSessionToken token,
            String url, boolean shouldSucceed) throws InterruptedException, TimeoutException {
        CustomTabsTestUtils.warmUpAndWait();
        if (token == null) {
            token = CustomTabsSessionToken.createMockSessionTokenForTesting();
            mCustomTabsConnection.newSession(token);
        }
        Uri uri = url == null ? null : Uri.parse(url);
        boolean succeeded = mCustomTabsConnection.mayLaunchUrl(token, uri, null, null);
        Assert.assertEquals(shouldSucceed, succeeded);
        return shouldSucceed ? token : null;
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(
     * CustomTabsSessionToken, Uri, android.os.Bundle, java.util.List)}
     * returns an error when called with an invalid session ID.
     */
    @Test
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidSessionId()
            throws InterruptedException, TimeoutException {
        assertWarmupAndMayLaunchUrl(
                CustomTabsSessionToken.createMockSessionTokenForTesting(), URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(CustomTabsSessionToken, Uri, Bundle, List)}
     * rejects invalid URL schemes.
     */
    @Test
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidScheme()
            throws InterruptedException, TimeoutException {
        assertWarmupAndMayLaunchUrl(null, INVALID_SCHEME_URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(CustomTabsSessionToken, Uri, Bundle, List)}
     * succeeds.
     */
    @Test
    @SmallTest
    public void testMayLaunchUrl() throws InterruptedException, TimeoutException {
        assertWarmupAndMayLaunchUrl(null, URL, true);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(CustomTabsSessionToken, Uri, Bundle, List)}
     * can be called several times with the same, and different URLs.
     */
    @Test
    @SmallTest
    public void testMultipleMayLaunchUrl() throws InterruptedException, TimeoutException {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        assertWarmupAndMayLaunchUrl(token, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        assertWarmupAndMayLaunchUrl(token, URL2, true);
    }

    /**
     * Tests that sessions are forgotten properly.
     */
    @Test
    @SmallTest
    public void testForgetsSession() throws InterruptedException, TimeoutException {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        CustomTabsTestUtils.cleanupSessions(mCustomTabsConnection);
        assertWarmupAndMayLaunchUrl(token, URL, false);
    }

    /**
     * Tests that CPU cgroups exist and have the expected values for background and foreground.
     *
     * To make testing easier the test assumes that the Android Framework uses
     * the same cgroup for background processes and background _threads_, which
     * is the the case between LOLLIPOP_MR1 and O.
     */
    @Test
    @SmallTest
    public void testGetSchedulerGroup() throws Exception {
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP);
        Assert.assertNotNull(CustomTabsConnection.getSchedulerGroup(Process.myPid()));
        String cgroup = CustomTabsConnection.getSchedulerGroup(Process.myPid());
        // Tests run in the foreground. Last two are from Android O.
        List<String> foregroundGroups = Arrays.asList("/", "/apps", "/top-app", "/foreground");
        Assert.assertTrue(foregroundGroups.contains(cgroup));

        // On O, a background thread is still in the foreground cgroup.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) return;
        final AtomicReference<String> backgroundThreadCgroup = new AtomicReference<>();
        Thread backgroundThread = new Thread(() -> {
            int tid = Process.myTid();
            Process.setThreadPriority(tid, Process.THREAD_PRIORITY_BACKGROUND);
            backgroundThreadCgroup.set(CustomTabsConnection.getSchedulerGroup(tid));
        });
        backgroundThread.start();
        backgroundThread.join();
        String threadCgroup = backgroundThreadCgroup.get();
        Assert.assertNotNull(threadCgroup);
        Assert.assertTrue(CustomTabsConnection.BACKGROUND_GROUPS.contains(threadCgroup));
    }

    /**
     * Tests that predictions are throttled.
     */
    @Test
    @SmallTest
    public void testThrottleMayLaunchUrl() throws InterruptedException, TimeoutException {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        int successfulRequests = 0;
        // Send a burst of requests instead of checking for precise delays to avoid flakiness.
        while (successfulRequests < 10) {
            if (!mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null)) break;
            successfulRequests++;
        }
        Assert.assertTrue("10 requests in a row should not all succeed.", successfulRequests < 10);
    }

    /**
     * Tests that the mayLaunchUrl() throttling is reset after a long enough wait.
     */
    @Test
    @SmallTest
    public void testThrottlingIsReset() throws InterruptedException, TimeoutException {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        // Depending on the timing, the delay should be 100 or 200ms here.
        assertWarmupAndMayLaunchUrl(token, URL, true);
        // assertWarmUpAndMayLaunchUrl() can take longer than the throttling delay.
        Assert.assertFalse(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        // Wait for more than 2 * MAX_POSSIBLE_DELAY to clear the delay
        try {
            Thread.sleep(450); // 2 * MAX_POSSIBLE_DELAY + 50ms
        } catch (InterruptedException e) {
            Assert.fail();
            return;
        }
        assertWarmupAndMayLaunchUrl(token, URL, true);
        // Check that the delay has been reset, by waiting for 100ms.
        try {
            Thread.sleep(150); // MIN_DELAY + 50ms margin
        } catch (InterruptedException e) {
            Assert.fail();
            return;
        }
        assertWarmupAndMayLaunchUrl(token, URL, true);
    }

    /**
     * Tests that throttling applies across sessions.
     */
    @Test
    @SmallTest
    public void testThrottlingAcrossSessions() throws InterruptedException, TimeoutException {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        CustomTabsSessionToken token2 = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        for (int i = 0; i < 10; i++) {
            mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null);
        }
        Assert.assertFalse(mCustomTabsConnection.mayLaunchUrl(token2, Uri.parse(URL), null, null));
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testBanningWorks() {
        mCustomTabsConnection.ban(Process.myUid());
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));

        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                Assert.assertNull(mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer));
            }
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testBanningDisabledForCellular() {
        mCustomTabsConnection.ban(Process.myUid());
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldPrerenderOnCellularForSession(token, true);

        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertNull(WarmupManager.getInstance().takeSpareWebContents(false, false));
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                WebContents prerender = mCustomTabsConnection.takePrerenderedUrl(
                        token, URL, referrer);
                Assert.assertNotNull(prerender);
                prerender.destroy();
            }
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCellularPrerenderingDoesntOverrideSettings() throws Exception {
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldPrerenderOnCellularForSession(token, true);
        CustomTabsTestUtils.warmUpAndWait();

        // Needs the browser process to be initialized.
        FutureTask<Boolean> result = ThreadUtils.runOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                PrefServiceBridge prefs = PrefServiceBridge.getInstance();
                boolean result = prefs.getNetworkPredictionEnabled();
                prefs.setNetworkPredictionEnabled(false);
                return result;
            }});
        final boolean enabled = result.get();

        try {
            Assert.assertTrue(
                    mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    assertSpareWebContentsNotNullAndDestroy();
                }
            });
        } finally {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    PrefServiceBridge.getInstance().setNetworkPredictionEnabled(enabled);
                }
            });
        }
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testCellularPrerenderingDoesntKillSpareRenderer() throws Exception {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldPrerenderOnCellularForSession(token, true);
        CustomTabsTestUtils.warmUpAndWait();

        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertTrue(WarmupManager.getInstance().hasSpareWebContents());
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                WebContents webContents =
                        mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer);
                Assert.assertNotNull(webContents);
                webContents.destroy();
                // destroyed when the prerender is used.
                Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
            }
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testUnmatchedCellularPrerenderingDoesntKillSpareRenderer() throws Exception {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldPrerenderOnCellularForSession(token, true);
        CustomTabsTestUtils.warmUpAndWait();

        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertTrue(WarmupManager.getInstance().hasSpareWebContents());
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                Assert.assertNull(mCustomTabsConnection.takePrerenderedUrl(token, URL2, referrer));
                Assert.assertTrue(WarmupManager.getInstance().hasSpareWebContents());
            }
        });
    }
}
