// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Application;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Process;
import android.support.customtabs.ICustomTabsCallback;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;

/** Tests for CustomTabsConnection. */
public class CustomTabsConnectionTest extends InstrumentationTestCase {
    private CustomTabsConnection mCustomTabsConnection;
    private static final String URL = "http://www.google.com";
    private static final String URL2 = "https://www.android.com";
    private static final String INVALID_SCHEME_URL = "intent://www.google.com";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        mCustomTabsConnection = CustomTabsConnection.getInstance((Application) context);
        mCustomTabsConnection.resetThrottling(Process.myUid());
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        cleanupSessions();
    }

    private void cleanupSessions() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mCustomTabsConnection.cleanupAll();
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
        ICustomTabsCallback cb = CustomTabsTestUtils.newDummyCallback();
        assertEquals(true, mCustomTabsConnection.newSession(cb));
        assertEquals(false, mCustomTabsConnection.newSession(cb));
    }

    /**
     * Tests that we can create several sessions.
     */
    @SmallTest
    public void testSeveralSessions() {
        ICustomTabsCallback cb = CustomTabsTestUtils.newDummyCallback();
        assertEquals(true, mCustomTabsConnection.newSession(cb));
        ICustomTabsCallback cb2 = CustomTabsTestUtils.newDummyCallback();
        assertEquals(true, mCustomTabsConnection.newSession(cb2));
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

    /**
     * Calls warmup() and mayLaunchUrl(), checks for the expected result
     * (success or failure) and returns the result code.
     */
    private ICustomTabsCallback assertWarmupAndMayLaunchUrl(
            ICustomTabsCallback cb, String url, boolean shouldSucceed) {
        mCustomTabsConnection.warmup(0);
        if (cb == null) {
            cb = CustomTabsTestUtils.newDummyCallback();
            mCustomTabsConnection.newSession(cb);
        }
        boolean succeeded = mCustomTabsConnection.mayLaunchUrl(cb, Uri.parse(url), null, null);
        assertEquals(shouldSucceed, succeeded);
        return shouldSucceed ? cb : null;
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(
     * ICustomTabsCallback, Uri, android.os.Bundle, java.util.List)}
     * returns an error when called with an invalid session ID.
     */
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidSessionId() {
        assertWarmupAndMayLaunchUrl(CustomTabsTestUtils.newDummyCallback(), URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(long, String, Bundle, List<Bundle>)}
     * rejects invalid URL schemes.
     */
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidScheme() {
        assertWarmupAndMayLaunchUrl(null, INVALID_SCHEME_URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(long, String, Bundle, List<Bundle>)}
     * succeeds.
     */
    @SmallTest
    public void testMayLaunchUrl() {
        assertWarmupAndMayLaunchUrl(null, URL, true);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(long, String, Bundle, List<Bundle>)}
     * can be called several times with the same, and different URLs.
     */
    @SmallTest
    public void testMultipleMayLaunchUrl() {
        ICustomTabsCallback cb = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        assertWarmupAndMayLaunchUrl(cb, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        assertWarmupAndMayLaunchUrl(cb, URL2, true);
    }

    /**
     * Tests that sessions are forgotten properly.
     */
    @SmallTest
    public void testForgetsSession() {
        ICustomTabsCallback cb = assertWarmupAndMayLaunchUrl(null, URL, true);
        cleanupSessions();
        assertWarmupAndMayLaunchUrl(cb, URL, false);
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
        ICustomTabsCallback cb = assertWarmupAndMayLaunchUrl(null, URL, true);
        int successfulRequests = 0;
        // Send a burst of requests instead of checking for precise delays to avoid flakiness.
        while (successfulRequests < 10) {
            if (!mCustomTabsConnection.mayLaunchUrl(cb, Uri.parse(URL), null, null)) break;
            successfulRequests++;
        }
        assertTrue("10 requests in a row should not all succeed.", successfulRequests < 10);
    }

    /**
     * Tests that the mayLaunchUrl() throttling is reset after a long enough wait.
     */
    @SmallTest
    public void testThrottlingIsReset() {
        ICustomTabsCallback cb = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.mayLaunchUrl(cb, Uri.parse(URL), null, null);
        // Depending on the timing, the delay should be 100 or 200ms here.
        assertWarmupAndMayLaunchUrl(cb, URL, false);
        // Wait for more than 2 * MAX_POSSIBLE_DELAY to clear the delay
        try {
            Thread.sleep(450); // 2 * MAX_POSSIBLE_DELAY + 50ms
        } catch (InterruptedException e) {
            fail();
            return;
        }
        assertWarmupAndMayLaunchUrl(cb, URL, true);
        // Check that the delay has been reset, by waiting for 100ms.
        try {
            Thread.sleep(150); // MIN_DELAY + 50ms margin
        } catch (InterruptedException e) {
            fail();
            return;
        }
        assertWarmupAndMayLaunchUrl(cb, URL, true);
    }

    /**
     * Tests that throttling applies across sessions.
     */
    @SmallTest
    public void testThrottlingAcrossSessions() {
        ICustomTabsCallback cb = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        ICustomTabsCallback cb2 = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        for (int i = 0; i < 10; i++) {
            mCustomTabsConnection.mayLaunchUrl(cb, Uri.parse(URL), null, null);
        }
        assertWarmupAndMayLaunchUrl(cb2, URL, false);
    }
}
