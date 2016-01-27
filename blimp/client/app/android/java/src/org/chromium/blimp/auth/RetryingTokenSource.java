// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.auth;

import android.content.Intent;
import android.os.Handler;
import android.os.Message;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

import java.util.Random;

/**
 * Wraps an existing {@link TokenSource} and adds exponential fallback retry support to it.  The
 * underlying {@link TokenSource} will be queried and all calls will be proxied except transient
 * failures, which will cause a retry after a random exponentially increasing delay.
 *
 * Because callbacks to {@link TokenSource#Callback#onTokenUnavailable(boolean)} with
 * {@code isTransient} set to {@code true} will be captured here, this {@link TokenSource} currently
 * won't expose any transient errors to the caller.
 */
public class RetryingTokenSource extends Handler implements TokenSource, TokenSource.Callback {
    private static final int MSG_QUERY_TOKEN = 1;
    private static final int BASE_BACKOFF_DELAY_MS = 500;
    private static final int MAX_EXPONENT = 10;

    private static final Random sRandom = new Random();

    /** The maximum number of times to attempt connection before failing. */
    @VisibleForTesting
    public static final int MAX_NUMBER_OF_RETRIES = 8;

    private final TokenSource mTokenSource;

    private TokenSource.Callback mCallback;
    private int mAttemptNumber;

    /**
     * Creates a {@link RetryingTokenSource} that proxies most {@link TokenSource} communication to
     * {@code tokenSource}.
     * @param tokenSource A {@link TokenSource} that does the actual underlying token management.
     */
    public RetryingTokenSource(TokenSource tokenSource) {
        mTokenSource = tokenSource;
        mTokenSource.setCallback(this);
    }

    // TokenSource implementation.
    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();

        mTokenSource.destroy();
        removeMessages(MSG_QUERY_TOKEN);
    }

    @Override
    public void setCallback(TokenSource.Callback callback) {
        ThreadUtils.assertOnUiThread();

        mCallback = callback;
    }

    @Override
    public void getToken() {
        ThreadUtils.assertOnUiThread();

        // Reset all exponential backoff states.
        removeMessages(MSG_QUERY_TOKEN);
        mAttemptNumber = 0;

        // Start the TokenSource#getToken() exponential backoff calls.
        getTokenWithBackoff();
    }

    @Override
    public boolean isRetrievingToken() {
        ThreadUtils.assertOnUiThread();

        return mTokenSource.isRetrievingToken() || hasMessages(MSG_QUERY_TOKEN);
    }

    @Override
    public int tokenIsInvalid(String token) {
        ThreadUtils.assertOnUiThread();

        return mTokenSource.tokenIsInvalid(token);
    }

    @Override
    public void onAccountSelected(Intent data) {
        ThreadUtils.assertOnUiThread();

        mTokenSource.onAccountSelected(data);
    }

    // TokenSource.Callback implementation.
    @Override
    public void onTokenReceived(String token) {
        mCallback.onTokenReceived(token);
    }

    @Override
    public void onTokenUnavailable(boolean isTransient) {
        if (isTransient && mAttemptNumber < MAX_NUMBER_OF_RETRIES) {
            getTokenWithBackoff();
        } else {
            mCallback.onTokenUnavailable(false);
        }
    }

    @Override
    public void onNeedsAccountToBeSelected(Intent intent) {
        mCallback.onNeedsAccountToBeSelected(intent);
    }

    // Handler overrides.
    @Override
    public void handleMessage(Message msg) {
        if (msg.what != MSG_QUERY_TOKEN) return;
        mTokenSource.getToken();
    }

    /**
     * @param delay The suggested time (in ms) to wait before attempting to query for the token
     *              again.
     * @return      The actual time (in ms) to wait before attempting to query for a token again.
     */
    @VisibleForTesting
    protected int finalizeRetryDelay(int delay) {
        return delay;
    }

    private void getTokenWithBackoff() {
        int delayMs = 0;

        // For the first attempt, don't delay.
        if (mAttemptNumber > 0) {
            // Find a random value between the previous and current max delay values.
            int prevMaxDelay = getMaxDelay(mAttemptNumber - 1);
            int currMaxDelay = getMaxDelay(mAttemptNumber);

            assert currMaxDelay > prevMaxDelay;
            int delayWindow = currMaxDelay - prevMaxDelay;

            delayMs = sRandom.nextInt(delayWindow) + prevMaxDelay;
        }

        sendEmptyMessageDelayed(MSG_QUERY_TOKEN, finalizeRetryDelay(delayMs));
        mAttemptNumber++;
    }

    /**
     * Helper method for calculating the max delay for any given attempt.
     * @param attempt The current attempt at calling {@link TokenSource#getToken()} on the internal
     *                {@link TokenSource}
     * @return        The maximum possible delay (in ms) to use for this attempt.
     */
    private static int getMaxDelay(int attempt) {
        // For the first attempt, use no delay.
        if (attempt == 0) return 0;

        // Figure out the delay multiplier 2^(retry attempt number).
        int multiplier = 1 << Math.min(MAX_EXPONENT, attempt - 1);
        return multiplier * BASE_BACKOFF_DELAY_MS;
    }
}