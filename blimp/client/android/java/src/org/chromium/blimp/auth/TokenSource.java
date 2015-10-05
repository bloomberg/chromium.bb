// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.auth;

import android.content.Intent;

import com.google.android.gms.common.ConnectionResult;

/**
 * Interface for an object that can asynchronously retrieve and invalidate user authentication
 * tokens.
 */
public interface TokenSource {
    /**
     * Used to get results of {@link TokenSource#getToken()} attempts.
     */
    public interface Callback {
        /**
         * A token was successfully received.
         * @param token The token.
         */
        void onTokenReceived(String token);

        /**
         * A token was unable to be retrieved.
         * @param isTransient Whether or not the failure is recoverable or not.
         */
        void onTokenUnavailable(boolean isTransient);

        /**
         * There is no selected user account on the device.  By triggering {@code intent} with a
         * result (via {@link android.app.Activity#startActivityForResult(Intent, int)) the calling
         * {@code android.app.Activity} can show an account chooser to the user.  The resulting
         * {@link Intent} should be passed to the {@link TokenSource} via
         * {@link TokenSource#onAccountSelected(Intent)}.
         * @param intent The {@link Intent} to start to have the user select an account.
         */
        void onNeedsAccountToBeSelected(Intent intent);
    }

    /**
     * Destroys all internal state and stops (if possible) any outstanding
     * {@link TokenSource#getToken()} attempts.
     */
    void destroy();

    /**
     * TODO(dtrainor): Rework this to move the Account and Callback into getToken() (crbug/537728).
     * Sets the {@link Callback} that will be notified of the results of {@link getToken()} calls.
     * @param callback The {@link Callback} to notify.
     */
    void setCallback(Callback callback);

    /**
     * Starts off a process of getting an authentication token for the selected account for this
     * application.  If this is called before finishing, it should cancel the outstanding request
     * and start a new one.  See {@link Callback} for specific details of each possible callback
     * outcome.
     * - If no account is selected, {@link Callback#onNeedsAccountToBeSelected(Intent)} will be
     * called.
     * - If getting the token fails, {@link Callback#onTokenUnavailable(boolean)} will be called.
     * - If getting the token is successful, {@link Callback#onTokenReceived(String)} will be
     * called.
     */
    void getToken();

    /**
     * @return Whether or not this {@link TokenSource} is currently trying to retrieve a token.
     */
    boolean isRetrievingToken();

    /**
     * Notifies this {@link TokenSource} that the token it returned is invalid.  This won't
     * automatically trigger another {@link #getToken()} attempt.
     * @param token The token that is invalid and should be removed from the underlying token cache.
     * @return      The result code of the attempted invalidation (see {@link ConnectionResult}.
     */
    int tokenIsInvalid(String token);

    /**
     * Notifies this {@link TokenSource} of a response from the {@link Intent} sent through
     * {@link Callback#onNeedsAccountToBeSelected(Intent)}.  The selected account will be parsed and
     * saved.  This won't automatically trigger another {@link #getToken()} attempt.
     * @param data The response {@link Intent} data.
     */
    void onAccountSelected(Intent data);
}