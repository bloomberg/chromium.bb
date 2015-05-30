// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link ConnectivityChecker}.
 */
public class ConnectivityCheckerTest extends ConnectivityCheckerTestBase {
    private static class Callback implements ConnectivityChecker.ConnectivityCheckerCallback {
        private final Semaphore mSemaphore;
        private final AtomicBoolean mConnected = new AtomicBoolean();

        Callback(Semaphore semaphore) {
            mSemaphore = semaphore;
        }

        @Override
        public void onResult(boolean connected) {
            mConnected.set(connected);
            mSemaphore.release();
        }

        boolean isConnected() {
            return mConnected.get();
        }
    }

    private static URL getURLNoException(String url) {
        try {
            return new URL(url);
        } catch (MalformedURLException e) {
            return null;
        }
    }

    @MediumTest
    public void testNoContentShouldWorkSystemStack() throws Exception {
        executeTest(GENERATE_204_URL, true, TIMEOUT_MS, true);
    }

    @MediumTest
    public void testNoContentShouldWorkChromeStack() throws Exception {
        executeTest(GENERATE_204_URL, true, TIMEOUT_MS, false);
    }

    @MediumTest
    public void testSlowNoContentShouldNotWorkSystemStack() throws Exception {
        // Force quick timeout. The server will wait TIMEOUT_MS, so this triggers well before.
        executeTest(GENERATE_204_SLOW_URL, false, 100, true);
    }

    @MediumTest
    public void testSlowNoContentShouldNotWorkChromeStack() throws Exception {
        // Force quick timeout. The server will wait TIMEOUT_MS, so this triggers well before.
        executeTest(GENERATE_204_SLOW_URL, false, 100, false);
    }

    @MediumTest
    public void testHttpOKShouldFailSystemStack() throws Exception {
        executeTest(GENERATE_200_URL, false, TIMEOUT_MS, true);
    }

    @MediumTest
    public void testHttpOKShouldFailChromeStack() throws Exception {
        executeTest(GENERATE_200_URL, false, TIMEOUT_MS, false);
    }

    @MediumTest
    public void testMovedTemporarilyShouldFailSystemStack() throws Exception {
        executeTest(GENERATE_302_URL, false, TIMEOUT_MS, true);
    }

    @MediumTest
    public void testMovedTemporarilyShouldFailChromeStack() throws Exception {
        executeTest(GENERATE_302_URL, false, TIMEOUT_MS, false);
    }

    @MediumTest
    public void testNotFoundShouldFailSystemStack() throws Exception {
        executeTest(GENERATE_404_URL, false, TIMEOUT_MS, true);
    }

    @MediumTest
    public void testNotFoundShouldFailChromeStack() throws Exception {
        executeTest(GENERATE_404_URL, false, TIMEOUT_MS, false);
    }

    @MediumTest
    public void testInvalidURLShouldFailSystemStack() throws Exception {
        executeTest("http:google.com:foo", false, TIMEOUT_MS, true);
    }

    @MediumTest
    public void testInvalidURLShouldFailChromeStack() throws Exception {
        executeTest("http:google.com:foo", false, TIMEOUT_MS, false);
    }

    private void executeTest(final String url, boolean expectedResult, final int timeoutMs,
            final boolean useSystemStack) throws Exception {
        Semaphore semaphore = new Semaphore(0);
        final Callback callback = new Callback(semaphore);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (useSystemStack) {
                    ConnectivityChecker.checkConnectivitySystemNetworkStack(
                            getURLNoException(url), timeoutMs, callback);
                } else {
                    ConnectivityChecker.checkConnectivityChromeNetworkStack(
                            Profile.getLastUsedProfile(), url, timeoutMs, callback);
                }
            }
        });

        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        assertEquals("URL: " + url + ", got " + callback.isConnected() + ", want " + expectedResult,
                expectedResult, callback.isConnected());

    }
}
