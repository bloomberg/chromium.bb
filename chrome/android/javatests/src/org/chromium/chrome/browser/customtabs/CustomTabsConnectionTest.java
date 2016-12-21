// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;

/** Tests for CustomTabsConnection. */
public class CustomTabsConnectionTest extends InstrumentationTestCase {
    private CustomTabsConnection mCustomTabsConnection;
    private static final String URL = "http://www.google.com";
    private static final String URL2 = "https://www.android.com";
    private static final String INVALID_SCHEME_URL = "intent://www.google.com";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";

    private Context mAppContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mAppContext = getInstrumentation().getTargetContext().getApplicationContext();
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        mCustomTabsConnection = CustomTabsTestUtils.setUpConnection((Application) mAppContext);
        mCustomTabsConnection.resetThrottling(mAppContext, Process.myUid());
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
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
    @SmallTest
    public void testNewSession() {
        assertEquals(false, mCustomTabsConnection.newSession(null));
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertEquals(true, mCustomTabsConnection.newSession(token));
        assertEquals(false, mCustomTabsConnection.newSession(token));
    }

    /**
     * Tests that we can create several sessions.
     */
    @SmallTest
    public void testSeveralSessions() {
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertEquals(true, mCustomTabsConnection.newSession(token));
        CustomTabsSessionToken token2 = CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertEquals(true, mCustomTabsConnection.newSession(token2));
    }

    /**
     * Tests that {@link CustomTabsConnection#warmup(long)} succeeds and can
     * be issued multiple times.
     */
    @SmallTest
    public void testCanWarmup() {
        assertEquals(true, mCustomTabsConnection.warmup(0));
        assertEquals(true, mCustomTabsConnection.warmup(0));
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCreateSpareRenderer() {
        assertTrue(mCustomTabsConnection.warmup(0));
        // On UI thread because:
        // 1. takeSpareWebContents needs to be called from the UI thread.
        // 2. warmup() is non-blocking and posts tasks to the UI thread, it ensures proper ordering.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                WarmupManager warmupManager = WarmupManager.getInstance();
                assertTrue(warmupManager.hasSpareWebContents());
                WebContents webContents = warmupManager.takeSpareWebContents(false, false);
                assertNotNull(webContents);
                assertFalse(warmupManager.hasSpareWebContents());
                webContents.destroy();
            }
        });
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCreateSpareRendererCanBeRecreated() {
        assertTrue(mCustomTabsConnection.warmup(0));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
                assertFalse(WarmupManager.getInstance().hasSpareWebContents());
            }
        });
        assertTrue(mCustomTabsConnection.warmup(0));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
            }
        });
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPrerenderDestroysSpareRenderer() {
        final CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse(WarmupManager.getInstance().hasSpareWebContents());
                String referrer =
                        mCustomTabsConnection.getReferrerForSession(token).getUrl();
                WebContents webContents =
                        mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer);
                assertNotNull(webContents);
                webContents.destroy();
            }
        });
    }

    /*
     * Tests that when the disconnection notification comes from a non-UI thread, Chrome doesn't
     * crash. Non-regression test for crbug.com/623128.
     */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPrerenderAndDisconnectOnOtherThread() {
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

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMayLaunchUrlKeepsSpareRendererWithoutPrerendering() {
        assertTrue(mCustomTabsConnection.warmup(0));
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertTrue(mCustomTabsConnection.newSession(token));

        Bundle extras = new Bundle();
        extras.putInt(
                CustomTabsConnection.DEBUG_OVERRIDE_KEY, CustomTabsConnection.NO_PRERENDERING);
        assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), extras, null));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
            }
        });
    }

    @SmallTest
    public void testMayLaunchUrlNullOrEmptyUrl() {
        assertWarmupAndMayLaunchUrl(null, null, true);
        CustomTabsTestUtils.cleanupSessions(mCustomTabsConnection); // Resets throttling.
        assertWarmupAndMayLaunchUrl(null, "", true);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testUnderstandsLowConfidenceMayLaunchUrl() {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertTrue(mCustomTabsConnection.newSession(token));
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
                assertNull(mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer));
            }
        });
    }

    @SmallTest
    public void testLowConfidenceMayLaunchUrlOnlyAcceptUris() {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertTrue(mCustomTabsConnection.newSession(token));
        assertTrue(mCustomTabsConnection.warmup(0));

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
                assertFalse(mCustomTabsConnection.lowConfidenceMayLaunchUrl(urlsAsString));
                assertTrue(mCustomTabsConnection.lowConfidenceMayLaunchUrl(urlsAsUri));
            }
        });
    }

    @SmallTest
    public void testLowConfidenceMayLaunchUrlDoesntCrash() {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertTrue(mCustomTabsConnection.newSession(token));
        assertTrue(mCustomTabsConnection.warmup(0));

        final List<Bundle> invalidBundles = new ArrayList<>();
        Bundle invalidBundle = new Bundle();
        invalidBundle.putParcelable(CustomTabsService.KEY_URL, new Intent());
        invalidBundles.add(invalidBundle);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mCustomTabsConnection.lowConfidenceMayLaunchUrl(invalidBundles);
                } catch (ClassCastException e) {
                    fail();
                }
            }
        });
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testStillHighConfidenceMayLaunchUrlWithSeveralUrls() {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertTrue(mCustomTabsConnection.newSession(token));
        List<Bundle> urls = new ArrayList<>();
        Bundle urlBundle = new Bundle();
        urlBundle.putParcelable(CustomTabsService.KEY_URL, Uri.parse(URL));
        urls.add(urlBundle);

        mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, urls);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertNull(WarmupManager.getInstance().takeSpareWebContents(false, false));
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                assertNotNull(mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer));
            }
        });
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPrefetchOnlyNoPrerenderHasSpareWebContents() {
        assertTrue(mCustomTabsConnection.warmup(0));
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertTrue(mCustomTabsConnection.newSession(token));

        Bundle extras = new Bundle();
        extras.putInt(CustomTabsConnection.DEBUG_OVERRIDE_KEY, CustomTabsConnection.PREFETCH_ONLY);
        assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), extras, null));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
            }
        });
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testCanCancelPrerender() {
        final CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertNull(WarmupManager.getInstance().takeSpareWebContents(false, false));
            }
        });
        assertTrue(mCustomTabsConnection.mayLaunchUrl(token, null, null, null));
        // mayLaunchUrl() posts a task, the following has to run after it.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                assertNull(mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer));
            }
        });
    }

    private void assertSpareWebContentsNotNullAndDestroy() {
        WebContents webContents = WarmupManager.getInstance().takeSpareWebContents(false, false);
        assertNotNull(webContents);
        webContents.destroy();
    }

    /**
     * Calls warmup() and mayLaunchUrl(), checks for the expected result
     * (success or failure) and returns the result code.
     */
    private CustomTabsSessionToken assertWarmupAndMayLaunchUrl(
            CustomTabsSessionToken token, String url, boolean shouldSucceed) {
        mCustomTabsConnection.warmup(0);
        if (token == null) {
            token = CustomTabsSessionToken.createDummySessionTokenForTesting();
            mCustomTabsConnection.newSession(token);
        }
        Uri uri = url == null ? null : Uri.parse(url);
        boolean succeeded = mCustomTabsConnection.mayLaunchUrl(token, uri, null, null);
        assertEquals(shouldSucceed, succeeded);
        return shouldSucceed ? token : null;
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(
     * CustomTabsSessionToken, Uri, android.os.Bundle, java.util.List)}
     * returns an error when called with an invalid session ID.
     */
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidSessionId() {
        assertWarmupAndMayLaunchUrl(
                CustomTabsSessionToken.createDummySessionTokenForTesting(), URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(CustomTabsSessionToken, Uri, Bundle, List)}
     * rejects invalid URL schemes.
     */
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidScheme() {
        assertWarmupAndMayLaunchUrl(null, INVALID_SCHEME_URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(CustomTabsSessionToken, Uri, Bundle, List)}
     * succeeds.
     */
    @SmallTest
    public void testMayLaunchUrl() {
        assertWarmupAndMayLaunchUrl(null, URL, true);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(CustomTabsSessionToken, Uri, Bundle, List)}
     * can be called several times with the same, and different URLs.
     */
    @SmallTest
    public void testMultipleMayLaunchUrl() {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(mAppContext, Process.myUid());
        assertWarmupAndMayLaunchUrl(token, URL, true);
        mCustomTabsConnection.resetThrottling(mAppContext, Process.myUid());
        assertWarmupAndMayLaunchUrl(token, URL2, true);
    }

    /**
     * Tests that sessions are forgotten properly.
     */
    @SmallTest
    public void testForgetsSession() {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        CustomTabsTestUtils.cleanupSessions(mCustomTabsConnection);
        assertWarmupAndMayLaunchUrl(token, URL, false);
    }

    /**
     * Tests that CPU cgroups exist and have the expected values for background and foreground.
     *
     * To make testing easier the test assumes that the Android Framework uses
     * the same cgroup for background processes and background _threads_, which
     * has been the case through LOLLIPOP_MR1.
     */
    @SmallTest
    public void testGetSchedulerGroup() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;
        assertNotNull(CustomTabsConnection.getSchedulerGroup(Process.myPid()));
        String cgroup = CustomTabsConnection.getSchedulerGroup(Process.myPid());
        // Tests run in the foreground.
        assertTrue(cgroup.equals("/") || cgroup.equals("/apps"));

        final String[] backgroundThreadCgroup = {null};
        Thread backgroundThread = new Thread(new Runnable() {
            @Override
            public void run() {
                int tid = Process.myTid();
                Process.setThreadPriority(tid, Process.THREAD_PRIORITY_BACKGROUND);
                backgroundThreadCgroup[0] = CustomTabsConnection.getSchedulerGroup(tid);
            }
        });
        backgroundThread.start();
        try {
            backgroundThread.join();
        } catch (InterruptedException e) {
            fail();
            return;
        }
        String threadCgroup = backgroundThreadCgroup[0];
        assertNotNull(threadCgroup);
        assertTrue(threadCgroup.equals("/bg_non_interactive")
                || threadCgroup.equals("/apps/bg_non_interactive"));
    }

    /**
     * Tests that predictions are throttled.
     */
    @SmallTest
    public void testThrottleMayLaunchUrl() {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        int successfulRequests = 0;
        // Send a burst of requests instead of checking for precise delays to avoid flakiness.
        while (successfulRequests < 10) {
            if (!mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null)) break;
            successfulRequests++;
        }
        assertTrue("10 requests in a row should not all succeed.", successfulRequests < 10);
    }

    /**
     * Tests that the mayLaunchUrl() throttling is reset after a long enough wait.
     */
    @SmallTest
    public void testThrottlingIsReset() {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null);
        // Depending on the timing, the delay should be 100 or 200ms here.
        assertWarmupAndMayLaunchUrl(token, URL, false);
        // Wait for more than 2 * MAX_POSSIBLE_DELAY to clear the delay
        try {
            Thread.sleep(450); // 2 * MAX_POSSIBLE_DELAY + 50ms
        } catch (InterruptedException e) {
            fail();
            return;
        }
        assertWarmupAndMayLaunchUrl(token, URL, true);
        // Check that the delay has been reset, by waiting for 100ms.
        try {
            Thread.sleep(150); // MIN_DELAY + 50ms margin
        } catch (InterruptedException e) {
            fail();
            return;
        }
        assertWarmupAndMayLaunchUrl(token, URL, true);
    }

    /**
     * Tests that throttling applies across sessions.
     */
    @SmallTest
    public void testThrottlingAcrossSessions() {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(mAppContext, Process.myUid());
        CustomTabsSessionToken token2 = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(mAppContext, Process.myUid());
        for (int i = 0; i < 10; i++) {
            mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null);
        }
        assertWarmupAndMayLaunchUrl(token2, URL, false);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testBanningWorks() {
        mCustomTabsConnection.ban(mAppContext, Process.myUid());
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertTrue(mCustomTabsConnection.newSession(token));

        assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertSpareWebContentsNotNullAndDestroy();
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                assertNull(mCustomTabsConnection.takePrerenderedUrl(token, URL, referrer));
            }
        });
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testBanningDisabledForCellular() {
        mCustomTabsConnection.ban(mAppContext, Process.myUid());
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertTrue(mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldPrerenderOnCellularForSession(token, true);

        assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertNull(WarmupManager.getInstance().takeSpareWebContents(false, false));
                String referrer = mCustomTabsConnection.getReferrerForSession(token).getUrl();
                WebContents prerender = mCustomTabsConnection.takePrerenderedUrl(
                        token, URL, referrer);
                assertNotNull(prerender);
                prerender.destroy();
            }
        });
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCellularPrerenderingDoesntOverrideSettings() throws Exception {
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        assertTrue(mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldPrerenderOnCellularForSession(token, true);
        mCustomTabsConnection.warmup(0);

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
            assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
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
}
