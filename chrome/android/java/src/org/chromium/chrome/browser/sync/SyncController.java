// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.util.Log;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.invalidation.InvalidationController;
import org.chromium.chrome.browser.signin.AccountManagementFragment;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInFlowObserver;
import org.chromium.chrome.browser.sync.ui.PassphraseActivity;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;

/**
 * SyncController handles the coordination of sync state between the invalidation controller,
 * the Android sync settings, and the native sync code.
 *
 * It also handles initialization of some pieces of sync state on startup.
 *
 * Sync state can be changed from four places:
 *
 * - The Chrome UI, which will call SyncController directly.
 * - Native sync, which can disable it via a dashboard stop and clear.
 * - Android's Chrome sync setting.
 * - Android's master sync setting.
 *
 * SyncController implements listeners for the last three cases. When master sync is disabled, we
 * are careful to not change the Android Chrome sync setting so we know whether to turn sync back
 * on when it is re-enabled.
 */
public class SyncController implements ApplicationStateListener,
        ProfileSyncService.SyncStateChangedListener,
        AndroidSyncSettings.AndroidSyncSettingsObserver {
    private static final String TAG = "SyncController";

    /**
     * An identifier for the generator in UniqueIdentificationGeneratorFactory to be used to
     * generate the sync sessions ID. The generator is registered in the Application's onCreate
     * method.
     */
    public static final String GENERATOR_ID = "SYNC";

    /**
     * Key for the delay_sync_setup preference.
     */
    private static final String DELAY_SYNC_SETUP_PREF = "delay_sync_setup";

    private static SyncController sInstance;

    private final Context mContext;
    private final ChromeSigninController mChromeSigninController;
    private final ProfileSyncService mProfileSyncService;
    private final SyncNotificationController mSyncNotificationController;

    /**
     * Denotes whether some extra work that's done only when the visible browser
     * is shown has been completed once. This is used for things that should be
     * done once per cold-start but only when the browser is visible.
     */
    private boolean mFirstActivityStarted = false;

    private SyncController(Context context) {
        mContext = context;
        mChromeSigninController = ChromeSigninController.get(mContext);
        AndroidSyncSettings.registerObserver(context, this);
        mProfileSyncService = ProfileSyncService.get(mContext);
        mProfileSyncService.addSyncStateChangedListener(this);

        mChromeSigninController.ensureGcmIsInitialized();

        // Set the sessions ID using the generator that was registered for GENERATOR_ID.
        mProfileSyncService.setSessionsId(
                UniqueIdentificationGeneratorFactory.getInstance(GENERATOR_ID));

        // Create the SyncNotificationController.
        mSyncNotificationController = new SyncNotificationController(
                mContext, PassphraseActivity.class, AccountManagementFragment.class);
        mProfileSyncService.addSyncStateChangedListener(mSyncNotificationController);

        updateSyncStateFromAndroid();

        ApplicationStatus.registerApplicationStateListener(this);
    }

    /**
     * Retrieve the singleton instance of this class.
     *
     * @param context the current context.
     * @return the singleton instance.
     */
    public static SyncController get(Context context) {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SyncController(context.getApplicationContext());
        }
        return sInstance;
    }

    /**
     * Trigger Chromium sign in of the given account.
     *
     * This also ensure that sync setup is not in progress anymore, so sync will start after
     * sync initialization has happened.
     *
     * @param activity the current activity.
     * @param accountName the full account name.
     */
    public void signIn(Activity activity, String accountName) {
        final Account account = AccountManagerHelper.createAccountFromName(accountName);

        // The SigninManager handles most of the sign-in flow, and doFinishSignIn handles the
        // ChromeShell specific details.
        SigninManager signinManager = SigninManager.get(mContext);
        signinManager.onFirstRunCheckDone();
        final boolean passive = false;
        signinManager.startSignIn(activity, account, passive, new SignInFlowObserver() {
            @Override
            public void onSigninComplete() {
                SigninManager.get(mContext).logInSignedInUser();
                mProfileSyncService.setSetupInProgress(false);
                mProfileSyncService.syncSignIn();
                start();
            }

            @Override
            public void onSigninCancelled() {
                stop();
            }
        });
    }

    /**
     * Updates sync to reflect the state of the Android sync settings.
     */
    public void updateSyncStateFromAndroid() {
        if (AndroidSyncSettings.isSyncEnabled(mContext)) {
            start();
        } else {
            stop();
        }
    }

    /**
     * Starts sync if the master sync flag is enabled.
     *
     * Affects native sync, the invalidation controller, and the Android sync settings.
     */
    public void start() {
        ThreadUtils.assertOnUiThread();
        if (AndroidSyncSettings.isMasterSyncEnabled(mContext)) {
            Log.d(TAG, "Enabling sync");
            InvalidationController.get(mContext).start();
            mProfileSyncService.enableSync();
            AndroidSyncSettings.enableChromeSync(mContext);
        }
    }

    /**
     * Stops Sync if a user is currently signed in.
     *
     * Affects native sync, the invalidation controller, and the Android sync settings.
     */
    public void stop() {
        ThreadUtils.assertOnUiThread();
        if (mChromeSigninController.isSignedIn()) {
            Log.d(TAG, "Disabling sync");
            InvalidationController.get(mContext).stop();
            mProfileSyncService.disableSync();
            if (AndroidSyncSettings.isMasterSyncEnabled(mContext)) {
                // Only disable Android's Chrome sync setting if we weren't disabled
                // by the master sync setting. This way, when master sync is enabled
                // they will both be on and sync will start again.
                AndroidSyncSettings.disableChromeSync(mContext);
            }
        }
    }

    /**
     * From {@link ProfileSyncService.SyncStateChangedListener}.
     *
     * Changes the invalidation controller and Android sync setting state to match
     * the new native sync state.
     */
    @Override
    public void syncStateChanged() {
        ThreadUtils.assertOnUiThread();
        // Make the Java state match the native state.
        if (mProfileSyncService.isSyncRequested()) {
            InvalidationController.get(mContext).start();
            AndroidSyncSettings.enableChromeSync(mContext);
        } else {
            InvalidationController.get(mContext).stop();
            if (AndroidSyncSettings.isMasterSyncEnabled(mContext)) {
                // See comment in stop().
                AndroidSyncSettings.disableChromeSync(mContext);
            }
        }
    }

    /**
     * From {@link AndroidSyncSettings.AndroidSyncSettingsObserver}.
     */
    @Override
    public void androidSyncSettingsChanged() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                updateSyncStateFromAndroid();
            }
        });
    }

    /**
     * Returns the SyncNotificationController.
     */
    public SyncNotificationController getSyncNotificationController() {
        return mSyncNotificationController;
    }

    @Override
    public void onApplicationStateChange(int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            onMainActivityStart();
        }
    }

    /**
     * Set the value of the delay_sync_setup preference.
     */
    public void setDelaySync(boolean delay) {
        PreferenceManager.getDefaultSharedPreferences(mContext).edit()
                .putBoolean(DELAY_SYNC_SETUP_PREF, delay).apply();
    }

    public void onMainActivityStart() {
        if (!mFirstActivityStarted) {
            onFirstStart();
        }
        if (mProfileSyncService.isFirstSetupInProgress()) {
            mProfileSyncService.setSyncSetupCompleted();
            SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(mContext);
            if (prefs.getBoolean(DELAY_SYNC_SETUP_PREF, false)) {
                mProfileSyncService.setSetupInProgress(false);
            }
        }
    }

    private void onFirstStart() {
        if (AndroidSyncSettings.isSyncEnabled(mContext)) {
            InvalidationController controller = InvalidationController.get(mContext);
            controller.refreshRegisteredTypes(mProfileSyncService.getPreferredDataTypes());
        }
        mFirstActivityStarted = true;
    }
}
