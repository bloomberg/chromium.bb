// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Handler;
import android.support.v7.app.AlertDialog;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.FieldTrialList;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.externalauth.UserRecoverableErrorHandler;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;

import javax.annotation.Nullable;

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
public class SigninManager implements AccountTrackerService.OnSystemAccountsSeededListener {
    public static final String CONFIRM_MANAGED_SIGNIN_DIALOG_TAG =
            "confirm_managed_signin_dialog_tag";

    private static final String TAG = "SigninManager";

    private static SigninManager sSigninManager;
    private static int sSignInAccessPoint = SigninAccessPoint.UNKNOWN;

    private final Context mContext;
    private final long mNativeSigninManagerAndroid;

    /** Tracks whether the First Run check has been completed.
     *
     * A new sign-in can not be started while this is pending, to prevent the
     * pending check from eventually starting a 2nd sign-in.
     */
    private boolean mFirstRunCheckIsPending = true;

    private final ObserverList<SignInStateObserver> mSignInStateObservers =
            new ObserverList<SignInStateObserver>();

    private final ObserverList<SignInAllowedObserver> mSignInAllowedObservers =
            new ObserverList<SignInAllowedObserver>();

    /**
    * Will be set during the sign in process, and nulled out when there is not a pending sign in.
    * Needs to be null checked after ever async entry point because it can be nulled out at any time
    * by system accounts changing.
    */
    private SignInState mSignInState;

    private Runnable mSignOutCallback;

    private boolean mSigninAllowedByPolicy;

    /**
     * A SignInStateObserver is notified when the user signs in to or out of Chrome.
     */
    public interface SignInStateObserver {
        /**
         * Invoked when the user has signed in to Chrome.
         */
        void onSignedIn();

        /**
         * Invoked when the user has signed out of Chrome.
         */
        void onSignedOut();
    }

    /**
     * SignInAllowedObservers will be notified once signing-in becomes allowed or disallowed.
     */
    public interface SignInAllowedObserver {
        /**
         * Invoked once all startup checks are done and signing-in becomes allowed, or disallowed.
         */
        void onSignInAllowedChanged();
    }

    /**
     * Callbacks for the sign-in flow.
     */
    public interface SignInCallback {
        /**
         * Invoked after sign-in is completed successfully.
         */
        void onSignInComplete();

        /**
         * Invoked if the sign-in processes does not complete for any reason.
         */
        void onSignInAborted();
    }

    /**
     * Hooks for wiping data during sign out.
     */
    public interface WipeDataHooks {
        /**
         * Called before data is wiped.
         */
        public void preWipeData();

        /**
         * Called after data is wiped.
         */
        public void postWipeData();
    }

    /**
     * Contains all the state needed for signin. This forces signin flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignInState {
        public final Account account;
        public final Activity activity;
        public final SignInCallback callback;

        public ConfirmManagedSigninFragment policyConfirmationDialog = null;

        /**
         * If the system accounts need to be seeded, the sign in flow will block for that to occur.
         * This boolean should be set to true during that time and then reset back to false
         * afterwards. This allows the manager to know if it should progress the flow when the
         * account tracker broadcasts updates.
         */
        public boolean blockedOnAccountSeeding = false;

        /**
         * @param account The account to sign in to.
         * @param activity Reference to the UI to use for dialogs. Null means forced signin.
         * @param callback Called when the sign-in process finishes or is cancelled. Can be null.
         */
        public SignInState(
                Account account, @Nullable Activity activity, @Nullable SignInCallback callback) {
            this.account = account;
            this.activity = activity;
            this.callback = callback;
        }

        /**
         * Returns whether this is an interactive sign-in flow.
         */
        public boolean isInteractive() {
            return activity != null;
        }

        /**
         * Returns whether the sign-in flow activity was set but is no longer valid.
         */
        private boolean isActivityDestroyed() {
            return activity != null
                    && ApplicationStatus.getStateForActivity(activity) == ActivityState.DESTROYED;
        }
    }

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
        mSigninAllowedByPolicy = nativeIsSigninAllowedByPolicy(mNativeSigninManagerAndroid);

        AccountTrackerService.get(mContext).addSystemAccountsSeededListener(this);
    }

    /**
    * Log the access point when the user see the view of choosing account to sign in.
    * @param accessPoint the enum value of AccessPoint defined in signin_metrics.h.
    */
    public static void logSigninStartAccessPoint(int accessPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninStartedAccessPoint", accessPoint, SigninAccessPoint.MAX);
        sSignInAccessPoint = accessPoint;
    }

    private void logSigninCompleteAccessPoint() {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninCompletedAccessPoint", sSignInAccessPoint, SigninAccessPoint.MAX);
        sSignInAccessPoint = SigninAccessPoint.UNKNOWN;
    }

    /**
     * Notifies the SigninManager that the First Run check has completed.
     *
     * The user will be allowed to sign-in once this is signaled.
     */
    public void onFirstRunCheckDone() {
        mFirstRunCheckIsPending = false;

        if (isSignInAllowed()) {
            notifySignInAllowedChanged();
        }
    }

    /**
     * Returns true if signin can be started now.
     */
    public boolean isSignInAllowed() {
        return mSigninAllowedByPolicy && !mFirstRunCheckIsPending && mSignInState == null
                && ChromeSigninController.get(mContext).getSignedInUser() == null;
    }

    /**
     * Returns true if signin is disabled by policy.
     */
    public boolean isSigninDisabledByPolicy() {
        return !mSigninAllowedByPolicy;
    }

    /**
     * Registers a SignInStateObserver to be notified when the user signs in or out of Chrome.
     */
    public void addSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.addObserver(observer);
    }

    /**
     * Unregisters a SignInStateObserver to be notified when the user signs in or out of Chrome.
     */
    public void removeSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.removeObserver(observer);
    }

    public void addSignInAllowedObserver(SignInAllowedObserver observer) {
        mSignInAllowedObservers.addObserver(observer);
    }

    public void removeSignInAllowedObserver(SignInAllowedObserver observer) {
        mSignInAllowedObservers.removeObserver(observer);
    }

    private void notifySignInAllowedChanged() {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                for (SignInAllowedObserver observer : mSignInAllowedObservers) {
                    observer.onSignInAllowedChanged();
                }
            }
        });
    }

    /**
    * Continue pending sign in after system accounts have been seeded into AccountTrackerService.
    */
    @Override
    public void onSystemAccountsSeedingComplete() {
        if (mSignInState != null && mSignInState.blockedOnAccountSeeding) {
            mSignInState.blockedOnAccountSeeding = false;
            progressSignInFlowInvestigateScenario();
        }
    }

    /**
    * Clear pending sign in when system accounts in AccountTrackerService were refreshed.
    */
    @Override
    public void onSystemAccountsChanged() {
        if (mSignInState != null) {
            abortSignIn();
        }
    }

    /**
     * Starts the sign-in flow, and executes the callback when finished.
     *
     * If an activity is provided, it is considered an "interactive" sign-in and the user can be
     * prompted to confirm various aspects of sign-in using dialogs inside the activity.
     * The sign-in flow goes through the following steps:
     *
     *   - Wait for AccountTrackerService to be seeded.
     *   - If interactive, confirm the account change with the user.
     *   - Wait for policy to be checked for the account.
     *   - If interactive and the account is managed, warn the user.
     *   - If managed, wait for the policy to be fetched.
     *   - Complete sign-in with the native SigninManager and kick off token requests.
     *   - Call the callback if provided.
     *
     * @param account The account to sign in to.
     * @param activity The activity used to launch UI prompts, or null for a forced signin.
     * @param callback Optional callback for when the sign-in process is finished.
     */
    public void signIn(
            Account account, @Nullable Activity activity, @Nullable SignInCallback callback) {
        if (account == null) {
            Log.w(TAG, "Ignoring sign-in request due to null account.");
            if (callback != null) callback.onSignInAborted();
            return;
        }

        if (mSignInState != null) {
            Log.w(TAG, "Ignoring sign-in request as another sign-in request is pending.");
            if (callback != null) callback.onSignInAborted();
            return;
        }

        if (mFirstRunCheckIsPending) {
            Log.w(TAG, "Ignoring sign-in request until the First Run check completes.");
            if (callback != null) callback.onSignInAborted();
            return;
        }

        mSignInState = new SignInState(account, activity, callback);
        notifySignInAllowedChanged();

        progressSignInFlowSeedSystemAccounts();
    }

    /**
     * Same as above but retrieves the Account object for the given accountName.
     */
    public void signIn(String accountName, @Nullable final Activity activity,
            @Nullable final SignInCallback callback) {
        AccountManagerHelper.get(mContext).getAccountFromName(accountName, new Callback<Account>() {
            @Override
            public void onResult(Account account) {
                signIn(account, activity, callback);
            }
        });
    }

    private void progressSignInFlowSeedSystemAccounts() {
        if (AccountTrackerService.get(mContext).checkAndSeedSystemAccounts()) {
            progressSignInFlowInvestigateScenario();
        } else if (AccountIdProvider.getInstance().canBeUsed(mContext)) {
            mSignInState.blockedOnAccountSeeding = true;
        } else {
            Activity activity = mSignInState.activity;
            UserRecoverableErrorHandler errorHandler = activity != null
                    ? new UserRecoverableErrorHandler.ModalDialog(activity)
                    : new UserRecoverableErrorHandler.SystemNotification();
            ExternalAuthUtils.getInstance().canUseGooglePlayServices(mContext, errorHandler);
            Log.w(TAG, "Cancelling the sign-in process as Google Play services is unavailable");
            abortSignIn();
        }
    }

    private void progressSignInFlowInvestigateScenario() {
        if (mSignInState.isInteractive()) {
            if (mSignInState.isActivityDestroyed()) {
                abortSignIn();
                return;
            }
            ConfirmAccountChangeFragment.confirmSyncAccount(
                    mSignInState.account.name, mSignInState.activity);
        } else {
            progressSignInFlowCheckPolicy();
        }
    }

    /**
     * Continues the signin flow by checking if there is a policy that the account will be subject
     * to. Unfortunately this method is package protected because Fragments cannot easily be given
     * callbacks to invoke upon user actions. This allows dialogs presented as part of the signin
     * investigation to continue the signin flow.
     */
    void progressSignInFlowCheckPolicy() {
        if (mSignInState == null) {
            Log.w(TAG, "Ignoring sign in progress request as no pending sign in.");
            return;
        }

        if (!nativeShouldLoadPolicyForUser(mSignInState.account.name)) {
            // Proceed with the sign-in flow without checking for policy if it can be determined
            // that this account can't have management enabled based on the username.
            finishSignIn();
            return;
        }

        Log.d(TAG, "Checking if account has policy management enabled");
        // This will call back to onPolicyCheckedBeforeSignIn.
        nativeCheckPolicyBeforeSignIn(mNativeSigninManagerAndroid, mSignInState.account.name);
    }

    @CalledByNative
    private void onPolicyCheckedBeforeSignIn(String managementDomain) {
        if (mSignInState == null) {
            Log.w(TAG, "Sign in request was canceled; aborting onPolicyCheckedBeforeSignIn().");
        }

        if (managementDomain == null) {
            Log.d(TAG, "Account doesn't have policy");
            finishSignIn();
            return;
        }

        if (mSignInState.isActivityDestroyed()) {
            abortSignIn();
            return;
        }

        if (!mSignInState.isInteractive()) {
            // If this is a forced sign-in then don't show the confirmation dialog.
            // This will call back to onPolicyFetchedBeforeSignIn.
            nativeFetchPolicyBeforeSignIn(mNativeSigninManagerAndroid);
            return;
        }

        Log.d(TAG, "Account has policy management");
        mSignInState.policyConfirmationDialog = new ConfirmManagedSigninFragment(
                managementDomain, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        if (mSignInState == null || mSignInState.policyConfirmationDialog == null) {
                            // Stop if sign-in was cancelled or this is a duplicate click event.
                            return;
                        }

                        mSignInState.policyConfirmationDialog = null;

                        switch (id) {
                            case AlertDialog.BUTTON_POSITIVE:
                                Log.d(TAG, "Accepted policy management, proceeding with sign-in.");
                                // This will call back to onPolicyFetchedBeforeSignIn.
                                nativeFetchPolicyBeforeSignIn(mNativeSigninManagerAndroid);
                                break;

                            default:
                                Log.d(TAG, "Policy confirmation rejected; abort sign-in.");
                                abortSignIn();
                                break;
                        }
                    }
                });
        mSignInState.policyConfirmationDialog.show(
                mSignInState.activity.getFragmentManager(), CONFIRM_MANAGED_SIGNIN_DIALOG_TAG);
    }

    @CalledByNative
    private void onPolicyFetchedBeforeSignIn() {
        // Policy has been fetched for the user and is being enforced; features like sync may now
        // be disabled by policy, and the rest of the sign-in flow can be resumed.
        finishSignIn();
    }

    private void finishSignIn() {
        if (mSignInState == null) {
            Log.w(TAG, "Sign in request was canceled; aborting finishSignIn().");
            return;
        }

        // Tell the native side that sign-in has completed.
        nativeOnSignInCompleted(mNativeSigninManagerAndroid, mSignInState.account.name);

        // Cache the signed-in account name. This must be done after the native call, otherwise
        // sync tries to start without being signed in natively and crashes.
        ChromeSigninController.get(mContext).setSignedInAccountName(mSignInState.account.name);

        if (mSignInState.callback != null) {
            mSignInState.callback.onSignInComplete();
        }

        // Trigger token requests via native.
        logInSignedInUser();

        if (mSignInState.isInteractive()) {
            // If signin was a user action, record that it succeeded.
            RecordUserAction.record("Signin_Signin_Succeed");
            logSigninCompleteAccessPoint();
            // Log signin in reason as defined in signin_metrics.h. Right now only
            // SIGNIN_PRIMARY_ACCOUNT available on Android.
            RecordHistogram.recordEnumeratedHistogram("Signin.SigninReason",
                    SigninReason.SIGNIN_PRIMARY_ACCOUNT, SigninReason.MAX);
        }

        Log.d(TAG, "Signin completed.");
        mSignInState = null;
        notifySignInAllowedChanged();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedIn();
        }
    }

    /**
     * Invokes signOut with no callback or wipeDataHooks.
     */
    public void signOut() {
        signOut(null, null);
    }

    /**
     * Invokes signOut() with no wipeDataHooks.
     */
    public void signOut(Runnable callback) {
        signOut(callback, null);
    }

    /**
     * Signs out of Chrome.
     * <p/>
     * This method clears the signed-in username, stops sync and sends out a
     * sign-out notification on the native side.
     *
     * @param callback Will be invoked after signout completes, if not null.
     * @param wipeDataHooks Hooks to call during data wiping in case the account is managed.
     */
    public void signOut(Runnable callback, WipeDataHooks wipeDataHooks) {
        mSignOutCallback = callback;

        boolean wipeData = getManagementDomain() != null;
        Log.d(TAG, "Signing out, wipe data? " + wipeData);

        ChromeSigninController.get(mContext).setSignedInAccountName(null);
        nativeSignOut(mNativeSigninManagerAndroid);

        if (wipeData) {
            wipeProfileData(wipeDataHooks);
        } else {
            onSignOutDone();
        }

        AccountTrackerService.get(mContext).invalidateAccountSeedStatus(true);
    }

    /**
     * Returns the management domain if the signed in account is managed, otherwise returns null.
     */
    public String getManagementDomain() {
        return nativeGetManagementDomain(mNativeSigninManagerAndroid);
    }

    public void logInSignedInUser() {
        nativeLogInSignedInUser(mNativeSigninManagerAndroid);
    }

    public void clearLastSignedInUser() {
        nativeClearLastSignedInUser(mNativeSigninManagerAndroid);
    }

    /**
     * Aborts the current sign in.
     *
     * Package protected to allow fragments to call into the signin flow.
     */
    void abortSignIn() {
        if (mSignInState == null) {
            Log.w(TAG, "Ignoring signin abort request as no pending sign in.");
            return;
        }

        if (mSignInState.policyConfirmationDialog != null) {
            mSignInState.policyConfirmationDialog.dismiss();
        }

        if (mSignInState.callback != null) {
            mSignInState.callback.onSignInAborted();
        }

        Log.d(TAG, "Signin flow aborted.");
        mSignInState = null;
        notifySignInAllowedChanged();
    }

    private void wipeProfileData(WipeDataHooks hooks) {
        if (hooks != null) hooks.preWipeData();
        // This will call back to onProfileDataWiped().
        nativeWipeProfileData(mNativeSigninManagerAndroid, hooks);
    }

    @CalledByNative
    private void onProfileDataWiped(WipeDataHooks hooks) {
        if (hooks != null) hooks.postWipeData();
        onSignOutDone();
    }

    private void onSignOutDone() {
        if (mSignOutCallback != null) {
            new Handler().post(mSignOutCallback);
            mSignOutCallback = null;
        }

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedOut();
        }
    }

    /**
     * @return Whether there is a signed in account on the native side.
     */
    public boolean isSignedInOnNative() {
        return nativeIsSignedInOnNative(mNativeSigninManagerAndroid);
    }

    /**
     * @return Experiment group for the android signin promo that the current user falls into.
     * -1 if the sigin promo experiment is disabled, otherwise an integer between 0 and 7.
     * TODO(guohui): instead of group names, it is better to use experiment params to control
     * the variations.
     */
    public static int getAndroidSigninPromoExperimentGroup() {
        String fieldTrialValue = FieldTrialList.findFullName("AndroidSigninPromo");
        try {
            return Integer.parseInt(fieldTrialValue);
        } catch (NumberFormatException ex) {
            return -1;
        }
    }

    @CalledByNative
    private void onSigninAllowedByPolicyChanged(boolean newSigninAllowedByPolicy) {
        mSigninAllowedByPolicy = newSigninAllowedByPolicy;
        notifySignInAllowedChanged();
    }

    // Native methods.
    private native long nativeInit();
    private native boolean nativeIsSigninAllowedByPolicy(long nativeSigninManagerAndroid);
    private native boolean nativeShouldLoadPolicyForUser(String username);
    private native void nativeCheckPolicyBeforeSignIn(
            long nativeSigninManagerAndroid, String username);
    private native void nativeFetchPolicyBeforeSignIn(long nativeSigninManagerAndroid);
    private native void nativeOnSignInCompleted(long nativeSigninManagerAndroid, String username);
    private native void nativeSignOut(long nativeSigninManagerAndroid);
    private native String nativeGetManagementDomain(long nativeSigninManagerAndroid);
    private native void nativeWipeProfileData(long nativeSigninManagerAndroid, WipeDataHooks hooks);
    private native void nativeClearLastSignedInUser(long nativeSigninManagerAndroid);
    private native void nativeLogInSignedInUser(long nativeSigninManagerAndroid);
    private native boolean nativeIsSignedInOnNative(long nativeSigninManagerAndroid);
}
