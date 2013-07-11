// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.InvalidationController;
import org.chromium.sync.notifier.SyncStatusHelper;
import org.chromium.sync.signin.ChromeSigninController;

import java.util.HashSet;

/**
 * Android wrapper of the SigninManager which provides access from the Java layer.
 * <p/>
 * This class handles common paths during the sign-in and sign-out flows.
 * <p/>
 * Only usable from the UI thread as the native SigninManager requires its access to be in the
 * UI thread.
 * <p/>
 * See chrome/browser/signin/signin_manager_android.h for more details.
 */
public class SigninManager {

    private static final String TAG = "SigninManager";

    private static SigninManager sSigninManager;

    private final Context mContext;
    private final int mNativeSigninManagerAndroid;

    private Runnable mSignInCallback;
    private Account mSignInAccount;
    private String mSyncToken;

    /**
     * A helper method for retrieving the application-wide SigninManager.
     * <p/>
     * Can only be accessed on the main thread.
     *
     * @param context the ApplicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the SigninManager.
     */
    public static SigninManager get(Context context) {
        ThreadUtils.assertOnUiThread();
        if (sSigninManager == null) {
            sSigninManager = new SigninManager(context);
        }
        return sSigninManager;
    }

    private SigninManager(Context context) {
        ThreadUtils.assertOnUiThread();
        mContext = context.getApplicationContext();
        mNativeSigninManagerAndroid = nativeInit();
    }

    /**
     * Starts the sign-in flow, and executes the callback when ready to proceed.
     * <p/>
     * This method checks with the native side whether the account has management enabled, and may
     * present a dialog to the user to confirm sign-in. The callback is invoked once these processes
     * and the common sign-in initialization complete.
     *
     * @param activity The context to use for the operation.
     * @param account The account to sign in to.
     * @param callback The {@link Runnable#run()} method is called once the operation completes.
     * The callback may be null, and is never invoked if the user cancels sign-in.
     */
    public void startSignIn(Activity activity, final Account account, final Runnable callback) {
        assert mSignInCallback == null;
        assert mSignInAccount == null;
        mSignInCallback = callback;
        mSignInAccount = account;

        if (!nativeShouldLoadPolicyForUser(account.name)) {
            // Proceed with the sign-in flow without checking for policy if it can be determined
            // that this account can't have management enabled based on the username.
            doSignIn();
            return;
        }

        Log.d(TAG, "Checking if account has policy management enabled");
        // This will call back to onPolicyCheckedBeforeSignIn.
        nativeCheckPolicyBeforeSignIn(mNativeSigninManagerAndroid, account.name);
    }

    @CalledByNative
    private void onPolicyCheckedBeforeSignIn(boolean hasPolicyManagement) {
        if (hasPolicyManagement) {
            Log.d(TAG, "Account has policy management");
            // TODO(joaodasilva): ask for confirmation before applying policy. The sign-in process
            // should be aborted if the user cancels at this step.
            // See FirstRunUtil.showToSDialog for an example.

            // This will call back to onPolicyFetchedBeforeSignIn.
            nativeFetchPolicyBeforeSignIn(mNativeSigninManagerAndroid);
        } else {
            Log.d(TAG, "Account doesn't have policy");
            doSignIn();
        }
    }

    @CalledByNative
    private void onPolicyFetchedBeforeSignIn() {
        // Policy has been fetched for the user and is being enforced; features like sync may now
        // be disabled by policy, and the rest of the sign-in flow can be resumed.
        doSignIn();
    }

    private void doSignIn() {
        Log.d(TAG, "Committing the sign-in process now");
        assert mSignInAccount != null;

        // Cache the signed-in account name.
        ChromeSigninController.get(mContext).setSignedInAccountName(mSignInAccount.name);

        // Tell the native side that sign-in has completed.
        // This will trigger NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL.
        nativeOnSignInCompleted(mNativeSigninManagerAndroid, mSignInAccount.name);

        // Register for invalidations.
        InvalidationController invalidationController = InvalidationController.get(mContext);
        invalidationController.setRegisteredTypes(mSignInAccount, true, new HashSet<ModelType>());

        // Sign-in to sync.
        ProfileSyncService profileSyncService = ProfileSyncService.get(mContext);
        if (SyncStatusHelper.get(mContext).isSyncEnabled(mSignInAccount) &&
                !profileSyncService.hasSyncSetupCompleted()) {
            profileSyncService.setSetupInProgress(true);
            profileSyncService.syncSignIn();
        }

        if (mSignInCallback != null)
            mSignInCallback.run();

        // All done, cleanup.
        Log.d(TAG, "Signin done");
        mSignInCallback = null;
        mSignInAccount = null;
    }

    /**
     * Signs out of Chrome.
     * <p/>
     * This method clears the signed-in username, stop sync and sends out a
     * sign-out notification on the native side.
     */
    public void signOut() {
        Log.d(TAG, "Signing out");
        ChromeSigninController.get(mContext).clearSignedInUser();
        ProfileSyncService.get(mContext).signOut();
        nativeSignOut(mNativeSigninManagerAndroid);
    }

    // Native methods.
    private native int nativeInit();
    private native boolean nativeShouldLoadPolicyForUser(String username);
    private native void nativeCheckPolicyBeforeSignIn(
            int nativeSigninManagerAndroid, String username);
    private native void nativeFetchPolicyBeforeSignIn(int nativeSigninManagerAndroid);
    private native void nativeOnSignInCompleted(int nativeSigninManagerAndroid, String username);
    private native void nativeSignOut(int nativeSigninManagerAndroid);
}
