// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.Account;
import android.app.Activity;

import org.chromium.chrome.browser.child_accounts.ChildAccountService;
import org.chromium.chrome.browser.signin.AccountManagementFragment;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInFlowObserver;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.SyncController;

/**
 * Helper functions for first run actions.
 */
public class FirstRunUtil {

    private FirstRunUtil() {}

    // The type of signin flow.
    /** Regular (interactive) signin. */
    public static final int SIGNIN_TYPE_INTERACTIVE = 0;

    /** Forced signin for education-enrolled devices. */
    public static final int SIGNIN_TYPE_FORCED_EDU = 1;

    /** Forced signin for child accounts. */
    public static final int SIGNIN_TYPE_FORCED_CHILD_ACCOUNT = 2;

    // The timing of enabling the ProfileSyncService.
    /** Postpone sync till the set up is fully complete. */
    public static final int SIGNIN_SYNC_SETUP_IN_PROGRESS = 0;

    /** Enable sync immediately. */
    public static final int SIGNIN_SYNC_IMMEDIATELY = 1;

    /**
     * Signs in to the specified account.
     * The operation will be performed in the background.
     *
     * @param activity   The context to use for the operation.
     * @param account    The account to sign into.
     * @param signInType The type of the sign-in (one of SIGNIN_TYPE constants).
     * @param signInSync When to enable the ProfileSyncService (one of SIGNIN_SYNC constants).
     * @param showSignInNotification Whether the sign-in notification should be shown.
     * @param observer   The observer to invoke when done, or null.
     */
    public static void signInToSelectedAccount(final Activity activity,
            final Account account,
            final int signInType,
            final int signInSync,
            final boolean showSignInNotification,
            final SignInFlowObserver observer) {
        // The SigninManager handles most of the sign-in flow, and onSigninComplete handles the
        // Chrome-specific details.
        SigninManager signinManager = SigninManager.get(activity);
        final boolean passive = signInType != SIGNIN_TYPE_INTERACTIVE;

        signinManager.startSignIn(activity, account, passive, new SignInFlowObserver() {
            @Override
            public void onSigninComplete() {
                // TODO(acleung): Maybe GoogleServicesManager should have a
                // sync = true but setSetupInProgress(true) state?
                ProfileSyncService.get(activity).setSetupInProgress(
                        signInSync == SIGNIN_SYNC_SETUP_IN_PROGRESS);
                SyncController.get(activity).start();

                if (observer != null) observer.onSigninComplete();

                if (signInType != SIGNIN_TYPE_INTERACTIVE) {
                    AccountManagementFragment.setSignOutAllowedPreferenceValue(activity, false);
                }

                if (signInType == SIGNIN_TYPE_FORCED_CHILD_ACCOUNT) {
                    ChildAccountService.getInstance(activity).onChildAccountSigninComplete();
                }

                SigninManager.get(activity).logInSignedInUser();
                // If Chrome was started from an external intent we should show the sync signin
                // popup, since the user has not seen the welcome screen where there is easy access
                // to turn off sync.
                if (showSignInNotification) {
                    ((FirstRunActivity) activity).showSignInNotification();
                }
            }
            @Override
            public void onSigninCancelled() {
                if (observer != null) observer.onSigninCancelled();
            }
        });
    }
}
