// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.auth;

import android.content.Intent;
import android.os.Handler;
import android.os.Message;

import com.google.android.gms.common.ConnectionResult;

import junit.framework.Assert;

/**
 * Test implementation of a TokenSource.  Has the following features:
 * - Can return a specific token.
 * - Can mimic a specific number of transient failures before a success or unrecoverable failure.
 * - Can mimic an unrecoverable failure.
 */
public class MockTokenSource extends Handler implements TokenSource {
    private static final int MSG_QUERY_TOKEN = 1;

    private final String mCorrectToken;

    /** Whether or not to fail in a non-transient way after all transient failures. */
    private final boolean mFailHard;

    private TokenSource.Callback mCallback;

    /**
     * The number of transient failures left to simulate before a successful or non-transient
     * failure is reported.
     */
    private int mTransientFailuresLeft;

    /** Whether or not a non-transient failure has already been reported. */
    private boolean mAlreadyFailedHard;

    /**
     * @param correctToken          The token to return on success.
     * @param transientFailureCount The number of transient failures to emit.
     * @param failsHardAfter        Whether or not to show a non-transient failure or successfully
     *                              return the token after all transient failures.
     */
    public MockTokenSource(String correctToken, int transientFailureCount, boolean failsHardAfter) {
        mCorrectToken = correctToken;
        mTransientFailuresLeft = transientFailureCount;
        mFailHard = failsHardAfter;
    }

    // TokenSource implementation.
    @Override
    public void destroy() {}

    @Override
    public void setCallback(TokenSource.Callback callback) {
        mCallback = callback;
    }

    @Override
    public void getToken() {
        Assert.assertFalse("getToken() called after already returning a successful token.",
                isRetrievingToken());
        Assert.assertFalse("getToken() called after failing in an unrecoverable way.",
                mAlreadyFailedHard);
        sendEmptyMessage(MSG_QUERY_TOKEN);
    }

    @Override
    public boolean isRetrievingToken() {
        return hasMessages(MSG_QUERY_TOKEN);
    }

    @Override
    public int tokenIsInvalid(String token) {
        return ConnectionResult.SUCCESS;
    }

    @Override
    public void onAccountSelected(Intent data) {}

    // Handler overrides.
    @Override
    public final void handleMessage(Message msg) {
        if (msg.what != MSG_QUERY_TOKEN) return;
        if (mTransientFailuresLeft > 0) {
            mCallback.onTokenUnavailable(true);
        } else if (mFailHard) {
            mAlreadyFailedHard = true;
            mCallback.onTokenUnavailable(false);
        } else {
            mCallback.onTokenReceived(mCorrectToken);
        }
        --mTransientFailuresLeft;
    }
}