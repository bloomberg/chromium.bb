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

    // The type of signin flow.
    /** Regular (interactive) signin. */
    public static final int SIGNIN_TYPE_INTERACTIVE = 0;

    /** Forced signin for education-enrolled devices. */
    public static final int SIGNIN_TYPE_FORCED_EDU = 1;

    /** Forced signin for child accounts. */
    public static final int SIGNIN_TYPE_FORCED_CHILD_ACCOUNT = 2;

    private static final String TAG = "SigninManager";

    private static SigninManager sSigninManager;

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

    private ConfirmManagedSigninFragment mPolicyConfirmationDialog;

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
     * Pass this observer to startSignIn() to be notified when sign-in completes or is canceled.
     */
    public interface SignInFlowObserver {
        /**
         * Invoked after sign-in completed successfully.
         */
        void onSigninComplete();

        /**
         * Invoked when the sign-in process was cancelled by the user.
         *
         * The user should have the option of going back and starting the process again,
         * if possible.
         */
        void onSigninCancelled();
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
     * An object aggregation class without business logic. This forces signin flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignInState {
        public final Activity activity;
        public final Account account;
        public final SignInFlowObserver observer;
        public final boolean passive;

        /**
         * If the system accounts need to be seeded, the sign in flow will block for that to occur.
         * This boolean should be set to true during that time and then reset back to false
         * afterwards. This allows the manager to know if it should progress the flow when the
         * account tracker broadcasts updates.
         */
        public boolean blockedOnAccountSeeding = false;

        /**
         * @param activity Reference to the UI to use for dialogs. Can be null.
         * @param account The account to sign in to.
         * @param observer The Observer to notify when the sign-in process finishes or is cancelled.
         * Can be null.
         * @param passive If passive is true then this operation should not interact with the user.
         */
        public SignInState(@Nullable final Activity activity, final Account account,
                @Nullable final SignInFlowObserver observer, final boolean passive) {
            this.activity = activity;
            this.account = account;
            this.observer = observer;
            this.passive = passive;
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
            cancelSignIn();
        }
    }

    /**
     * Shared logic that launches an interactive sign in flow asynchronously.
     *
     * @param accountName the id of the account that can be used to look it up, usually email.
     * @param activity the context to execute within, used to grab singletons.
     */
    public static void startInteractiveSignIn(final String accountName, final Activity activity) {
        if (activity == null) return;
        AccountManagerHelper.get(activity).getAccountFromName(accountName, new Callback<Account>() {
            @Override
            public void onResult(Account account) {
                if (account == null) return;
                SigninManager.get(activity).signInToSelectedAccount(activity, account,
                        SigninManager.SIGNIN_TYPE_INTERACTIVE, new SignInFlowObserver() {
                            @Override
                            public void onSigninComplete() {
                                RecordUserAction.record("Signin_Signin_Succeed");
                            }
                            @Override
                            public void onSigninCancelled() {}
                        });
            }
        });
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
     * @param passive If passive is true then this operation should not interact with the user.
     * @param observer The Observer to notify when the sign-in process is finished.
     */
    public void startSignIn(@Nullable Activity activity, final Account account, boolean passive,
            final SignInFlowObserver observer) {
        if (mSignInState != null) {
            Log.w(TAG, "Ignoring sign-in request as another sign-in request is pending.");
            return;
        }

        if (mFirstRunCheckIsPending) {
            Log.w(TAG, "Ignoring sign-in request until the First Run check completes.");
            return;
        }

        mSignInState = new SignInState(activity, account, observer, passive);
        notifySignInAllowedChanged();

        progressSignInFlowSeedSystemAccounts();
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
            cancelSignIn();
        }
    }

    private void progressSignInFlowInvestigateScenario() {
        if (!mSignInState.passive && mSignInState.activity != null) {
            if (isSignInActivityDestroyed()) {
                cancelSignIn();
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
     * investigation to externallys continue the signin flow.
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

        if (isSignInActivityDestroyed()) {
            cancelSignIn();
            return;
        }

        if (mSignInState.passive || mSignInState.activity == null) {
            // If this is a passive interaction (e.g. auto signin) then don't show the confirmation
            // dialog. This will call back to onPolicyFetchedBeforeSignIn.
            nativeFetchPolicyBeforeSignIn(mNativeSigninManagerAndroid);
            return;
        }

        Log.d(TAG, "Account has policy management");
        mPolicyConfirmationDialog = new ConfirmManagedSigninFragment(
                managementDomain,
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        if (mPolicyConfirmationDialog == null) return;
                        mPolicyConfirmationDialog = null;

                        switch (id) {
                            case AlertDialog.BUTTON_POSITIVE:
                                Log.d(TAG, "Accepted policy management, proceeding with sign-in");
                                // This will call back to onPolicyFetchedBeforeSignIn.
                                nativeFetchPolicyBeforeSignIn(mNativeSigninManagerAndroid);
                                break;

                            default:
                                Log.d(TAG, "Cancelled sign-in");
                                cancelSignIn();
                                break;
                        }
                    }
                });
        mPolicyConfirmationDialog.show(
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

        if (mSignInState.observer != null) {
            mSignInState.observer.onSigninComplete();
        }

        Log.d(TAG, "Signin completed.");
        mSignInState = null;
        notifySignInAllowedChanged();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedIn();
        }
    }

    /*
     * Returns if the sign in flows activity was set but is no longer valid. In
     * these cases the sign in flow should be aborted. If the activity started
     * with a null value then the current sign in should continue.
     */
    private boolean isSignInActivityDestroyed() {
        return mSignInState.activity != null
                && ApplicationStatus.getStateForActivity(mSignInState.activity)
                == ActivityState.DESTROYED;
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
     * Cancels the current sign in. TODO(skym): crbug.com/580145, we may be leaving a signin dialog
     * on the screen that will not function correctly anymore. Package protected to allow fragments
     * to call into the signin flow.
     */
    void cancelSignIn() {
        if (mSignInState == null) {
            Log.w(TAG, "Ignoring sign in cancel request as no pending sign in.");
            return;
        }

        if (mSignInState.observer != null) {
            mSignInState.observer.onSigninCancelled();
        }

        Log.d(TAG, "Signin cancelled.");
        mSignInState = null;
        notifySignInAllowedChanged();
    }

    private void wipeProfileData(WipeDataHooks hooks) {
        if (hooks != null) hooks.preWipeData();
        // This will call back to onProfileDataWiped().
        nativeWipeProfileData(mNativeSigninManagerAndroid, hooks);
    }

    /**
     * Signs in to the specified account. The operation will be performed in the background.
     *
     * @param activity   The activity to use to show UI (confirmation dialogs), or null for forced
     *                   signin.
     * @param account    The account to sign into.
     * @param signInType The type of the sign-in (one of SIGNIN_TYPE constants).
     * @param observer   The observer to invoke when done, or null.
     */
    public void signInToSelectedAccount(@Nullable Activity activity, final Account account,
            final int signInType, @Nullable final SignInFlowObserver observer) {
        // The SigninManager handles most of the sign-in flow, and onSigninComplete handles the
        // Chrome-specific details.
        final boolean passive = signInType != SIGNIN_TYPE_INTERACTIVE;

        startSignIn(activity, account, passive, new SignInFlowObserver() {
            @Override
            public void onSigninComplete() {
                if (observer != null) observer.onSigninComplete();

                if (signInType != SIGNIN_TYPE_INTERACTIVE) {
                    AccountManagementFragment.setSignOutAllowedPreferenceValue(mContext, false);
                }

                SigninManager.get(mContext).logInSignedInUser();
            }
            @Override
            public void onSigninCancelled() {
                if (observer != null) observer.onSigninCancelled();
            }
        });
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
        String fieldTrialValue =
                FieldTrialList.findFullName("AndroidSigninPromo");
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
