// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for {@link ConnectivityChecker}.
 */
public class ConnectivityCheckerTest extends ConnectivityCheckerTestBase {
    private static class Callback implements ConnectivityChecker.ConnectivityCheckerCallback {
        private final Semaphore mSemaphore;
        private final AtomicInteger mResult = new AtomicInteger();

        Callback(Semaphore semaphore) {
            mSemaphore = semaphore;
        }

        @Override
        public void onResult(int result) {
            mResult.set(result);
            mSemaphore.release();
        }

        int getResult() {
            return mResult.get();
        }
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testNoContentShouldWorkSystemStack() throws Exception {
        executeTest(mGenerate204Url, ConnectivityCheckResult.CONNECTED, TIMEOUT_MS, true);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testNoContentShouldWorkChromeStack() throws Exception {
        executeTest(mGenerate204Url, ConnectivityCheckResult.CONNECTED, TIMEOUT_MS, false);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testSlowNoContentShouldNotWorkSystemStack() throws Exception {
        // Force quick timeout. The server will wait TIMEOUT_MS, so this triggers well before.
        executeTest(mGenerateSlowUrl, ConnectivityCheckResult.TIMEOUT, 100, true);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testSlowNoContentShouldNotWorkChromeStack() throws Exception {
        // Force quick timeout. The server will wait TIMEOUT_MS, so this triggers well before.
        executeTest(mGenerateSlowUrl, ConnectivityCheckResult.TIMEOUT, 100, false);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testHttpOKShouldFailSystemStack() throws Exception {
        executeTest(mGenerate200Url, ConnectivityCheckResult.NOT_CONNECTED, TIMEOUT_MS, true);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testHttpOKShouldFailChromeStack() throws Exception {
        executeTest(mGenerate200Url, ConnectivityCheckResult.NOT_CONNECTED, TIMEOUT_MS, false);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testMovedTemporarilyShouldFailSystemStack() throws Exception {
        executeTest(mGenerate302Url, ConnectivityCheckResult.NOT_CONNECTED, TIMEOUT_MS, true);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testMovedTemporarilyShouldFailChromeStack() throws Exception {
        executeTest(mGenerate302Url, ConnectivityCheckResult.NOT_CONNECTED, TIMEOUT_MS, false);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testNotFoundShouldFailSystemStack() throws Exception {
        executeTest(mGenerate404Url, ConnectivityCheckResult.NOT_CONNECTED, TIMEOUT_MS, true);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testNotFoundShouldFailChromeStack() throws Exception {
        executeTest(mGenerate404Url, ConnectivityCheckResult.NOT_CONNECTED, TIMEOUT_MS, false);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testInvalidURLShouldFailSystemStack() throws Exception {
        executeTest("http:google.com:foo", ConnectivityCheckResult.ERROR, TIMEOUT_MS, true);
    }

    @MediumTest
    @Feature({"Feedback"})
    public void testInvalidURLShouldFailChromeStack() throws Exception {
        executeTest("http:google.com:foo", ConnectivityCheckResult.ERROR, TIMEOUT_MS, false);
    }

    private void executeTest(final String url, int expectedResult, final int timeoutMs,
            final boolean useSystemStack) throws Exception {
        Semaphore semaphore = new Semaphore(0);
        final Callback callback = new Callback(semaphore);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (useSystemStack) {
                    ConnectivityChecker.checkConnectivitySystemNetworkStack(
                            url, timeoutMs, callback);
                } else {
                    ConnectivityChecker.checkConnectivityChromeNetworkStack(
                            Profile.getLastUsedProfile(), url, timeoutMs, callback);
                }
            }
        });

        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        assertEquals("URL: " + url + ", got " + callback.getResult() + ", want " + expectedResult,
                expectedResult, callback.getResult());
    }
}
