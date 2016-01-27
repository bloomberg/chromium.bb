// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.auth;

import android.content.Intent;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests the basic behavior of a {@link RetryingTokenSource}.
 */
public class RetryingTokenSourceTest extends InstrumentationTestCase {
    private static final String TEST_TOKEN = "abcdefg";
    private static final int SEMAPHORE_TIMEOUT_MS = 4000;

    /**
     * A wrapper for a {@link RetryingTokenSource} that minimizes the retry delay for testing
     * purposes.  The original delay is still tracked.  This also implements a
     * {@link TokenSource#Callback} and notifies {@link Semaphore}s when certain callback methods
     * are called.
     */
    private static class TestRetryingTokenSource extends RetryingTokenSource {
        private final List<Integer> mRetryDelays = new ArrayList<Integer>();
        private final Semaphore mSuccessSemaphore;
        private final Semaphore mFailSemaphore;
        private final Semaphore mNeedsAccountSemaphore;

        /**
         * The token received from the underlying {@link TokenSource} or {@code null} if no token
         * was received.
         */
        private String mReceivedToken;

        private TokenSource.Callback mCallback = new TokenSource.Callback() {
            @Override
            public void onTokenReceived(String token) {
                mReceivedToken = token;
                if (mSuccessSemaphore != null) mSuccessSemaphore.release();
            }

            @Override
            public void onTokenUnavailable(boolean isTransient) {
                assertFalse("getToken() failed in a recoverable way for a RetryingTokenSource.",
                        isTransient);
                if (mFailSemaphore != null) mFailSemaphore.release();
            }

            @Override
            public void onNeedsAccountToBeSelected(Intent intent) {
                if (mNeedsAccountSemaphore != null) mNeedsAccountSemaphore.release();
            }
        };

        /**
         * @param tokenSource           The underlying {@link TokenSource} to use.
         * @param successSemaphore      A {@link Semaphore} to track calls to
         *                              {@link TokenSource#Callback#onTokenReceived(String)}.
         * @param failureSemaphore      A {@link Semaphore} to track calls to
         *                              {@link TokenSource#Callback#onTokenUnavailable(boolean)}.
         * @param needsAccountSemaphore A {@link Semaphore} to track calls to {@link
         *                              TokenSource#Callback#onNeedsAccountToBeSelected(Intent)}.
         */
        public TestRetryingTokenSource(TokenSource tokenSource, Semaphore successSemaphore,
                Semaphore failureSemaphore, Semaphore needsAccountSemaphore) {
            super(tokenSource);

            mSuccessSemaphore = successSemaphore;
            mFailSemaphore = failureSemaphore;
            mNeedsAccountSemaphore = needsAccountSemaphore;

            setCallback(mCallback);
        }

        /**
         * @return The token received from the underlying {@link TokenSource} or {@code null} if
         *         no token was received.
         */
        public String getReceivedToken() {
            return mReceivedToken;
        }

        /**
         * @return A {@link List} of the delays (in ms) that would have been between each retry
         *         attempt.
         */
        public List<Integer> getRetryDelays() {
            return mRetryDelays;
        }

        /**
         * Minimize the actual delay for testing purposes, but save the original delay to validate
         * that the backoff is working.
         * @param delay The original delay the {@link RetryingTokenSource} would like to use.
         * @return      A small delay to be used during testing.
         */
        @Override
        protected int finalizeRetryDelay(int delay) {
            mRetryDelays.add(delay);
            return 1;
        }
    }

    private AtomicReference<TestRetryingTokenSource> buildAndTriggerTokenSource(
            final String correctToken,
            final int transientFailures,
            final boolean hardFailure,
            final Semaphore successSemaphore,
            final Semaphore failureSemaphore,
            final Semaphore needsAccountSemaphore) {
        final AtomicReference<TestRetryingTokenSource> tokenSource =
                new AtomicReference<TestRetryingTokenSource>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TokenSource mockTokenSource =
                        new MockTokenSource(correctToken, transientFailures, hardFailure);
                tokenSource.set(new TestRetryingTokenSource(mockTokenSource,
                        successSemaphore, failureSemaphore, needsAccountSemaphore));
                tokenSource.get().getToken();
                assertTrue("RetryingTokenSource is not attempting to get a token.",
                        tokenSource.get().isRetrievingToken());
            }
        });
        return tokenSource;
    }

    private void validateTokenSourceResults(
            final AtomicReference<TestRetryingTokenSource> tokenSource,
            final String expectedToken,
            final int expectedTransientFailures) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                String token = tokenSource.get().getReceivedToken();
                assertEquals("getToken() resulted in the wrong token.", expectedToken, token);

                List<Integer> delays = tokenSource.get().getRetryDelays();
                assertEquals("getToken() resulted in an incorrect number of retries.",
                        expectedTransientFailures + 1, delays.size());

                Integer prevDelay = Integer.MIN_VALUE;
                for (int i = 0; i < delays.size(); i++) {
                    Integer delay = delays.get(i);
                    assertTrue("RetryingTokenSource did not increase delays between attempts "
                            + "(" + prevDelay + " < " + delay + " failed).",
                            prevDelay < delay);
                    assertTrue("RetryingTokenSource used a negative delay.",
                            delay >= 0);
                    assertTrue("RetryingTokenSource used no delay for retries.",
                            delay > 0 || i == 0);
                    prevDelay = delay;
                }
            }
        });
    }

    /**
     * Tests basic behavior when the token is returned successfully.
     * @throws InterruptedException
     */
    @SmallTest
    public void testSuccessfulTokenSource() throws InterruptedException {
        Semaphore success = new Semaphore(0);
        Semaphore failure = new Semaphore(0);
        Semaphore needsAccount = new Semaphore(0);
        AtomicReference<TestRetryingTokenSource> tokenSource =
                buildAndTriggerTokenSource(TEST_TOKEN, 0, false, success, failure, needsAccount);

        // Validate that the callbacks got the expected results.
        assertTrue("Did not receive a successful token in time.",
                success.tryAcquire(SEMAPHORE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        assertFalse("getToken() failed.", failure.tryAcquire());
        assertFalse("getToken() needed an account.", needsAccount.tryAcquire());

        validateTokenSourceResults(tokenSource, TEST_TOKEN, 0);
    }

    /**
     * Tests retry behavior when there is a transient error multiple times before the token is
     * returned successfully.
     * @throws InterruptedException
     */
    @SmallTest
    public void testRecoveringTokenSource() throws InterruptedException {
        int failureCount = 6;

        Semaphore success = new Semaphore(0);
        Semaphore failure = new Semaphore(0);
        Semaphore needsAccount = new Semaphore(0);
        AtomicReference<TestRetryingTokenSource> tokenSource =
                buildAndTriggerTokenSource(TEST_TOKEN, failureCount, false, success, failure,
                        needsAccount);

        assertTrue("Did not receive a successful token in time.",
                success.tryAcquire(SEMAPHORE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        assertFalse("getToken() failed.", failure.tryAcquire());
        assertFalse("getToken() needed an account.", needsAccount.tryAcquire());

        validateTokenSourceResults(tokenSource, TEST_TOKEN, failureCount);
    }

    /**
     * Tests failure behavior for when there is an unrecoverable error after multiple transient
     * errors.
     * @throws InterruptedException
     */
    @SmallTest
    public void testFailedTokenSource() throws InterruptedException {
        int failureCount = 6;

        Semaphore success = new Semaphore(0);
        Semaphore failure = new Semaphore(0);
        Semaphore needsAccount = new Semaphore(0);
        AtomicReference<TestRetryingTokenSource> tokenSource =
                buildAndTriggerTokenSource(TEST_TOKEN, failureCount, true, success, failure,
                        needsAccount);

        assertTrue("Did not receive a failure in time.",
                failure.tryAcquire(SEMAPHORE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        assertFalse("getToken() succeeded.", success.tryAcquire());
        assertFalse("getToken() needed an account.", needsAccount.tryAcquire());

        validateTokenSourceResults(tokenSource, null, failureCount);
    }

    /**
     * Tests failure behavior for when there is an unrecoverable error after multiple transient
     * errors.
     * @throws InterruptedException
     */
    @SmallTest
    public void testTooManyTransientFailures() throws InterruptedException {
        int failureCount = RetryingTokenSource.MAX_NUMBER_OF_RETRIES + 1;

        Semaphore success = new Semaphore(0);
        Semaphore failure = new Semaphore(0);
        Semaphore needsAccount = new Semaphore(0);
        AtomicReference<TestRetryingTokenSource> tokenSource =
                buildAndTriggerTokenSource(TEST_TOKEN, failureCount, false, success, failure,
                        needsAccount);

        assertTrue("Did not receive a failure in time.",
                failure.tryAcquire(SEMAPHORE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        assertFalse("getToken() succeeded.", success.tryAcquire());
        assertFalse("getToken() needed an account.", needsAccount.tryAcquire());

        // Expect one less transient error than MAX_NUMBER_OF_RETRIES.
        validateTokenSourceResults(
                tokenSource, null, RetryingTokenSource.MAX_NUMBER_OF_RETRIES - 1);
    }
}