// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.util.Log;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.invalidation.InvalidationController;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInFlowObserver;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;

/**
 * SyncController handles the coordination of sync state between the invalidation controller,
 * the Android sync settings, and the native sync code.
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
public class SyncController implements ProfileSyncService.SyncStateChangedListener,
        AndroidSyncSettings.AndroidSyncSettingsObserver {
    private static final String TAG = "SyncController";

    /**
     * An identifier for the generator in UniqueIdentificationGeneratorFactory to be used to
     * generate the sync sessions ID. The generator is registered in the Application's onCreate
     * method.
     */
    public static final String GENERATOR_ID = "SYNC";

    private static SyncController sInstance;

    private final Context mContext;
    private final ChromeSigninController mChromeSigninController;
    private final AndroidSyncSettings mAndroidSyncSettings;
    private final ProfileSyncService mProfileSyncService;
    // TODO(maxbogue): Make final once it's constructed in this class.
    private SyncNotificationController mSyncNotificationController = null;

    private SyncController(Context context) {
        mContext = context;
        mChromeSigninController = ChromeSigninController.get(mContext);
        mAndroidSyncSettings = AndroidSyncSettings.get(context);
        mAndroidSyncSettings.registerObserver(this);
        mProfileSyncService = ProfileSyncService.get(mContext);
        mProfileSyncService.addSyncStateChangedListener(this);
        mChromeSigninController.ensureGcmIsInitialized();
        // Set the sessions ID using the generator that was registered for GENERATOR_ID.
        mProfileSyncService.setSessionsId(
                UniqueIdentificationGeneratorFactory.getInstance(GENERATOR_ID));
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
        if (mAndroidSyncSettings.isSyncEnabled()) {
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
        if (mAndroidSyncSettings.isMasterSyncEnabled()) {
            Log.d(TAG, "Enabling sync");
            InvalidationController.get(mContext).start();
            mProfileSyncService.enableSync();
            mAndroidSyncSettings.enableChromeSync();
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
            if (mAndroidSyncSettings.isMasterSyncEnabled()) {
                // Only disable Android's Chrome sync setting if we weren't disabled
                // by the master sync setting. This way, when master sync is enabled
                // they will both be on and sync will start again.
                mAndroidSyncSettings.disableChromeSync();
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
        boolean isSyncActive = !mProfileSyncService.isStartSuppressed();
        // Make the Java state match the native state.
        if (isSyncActive) {
            InvalidationController.get(mContext).start();
            mAndroidSyncSettings.enableChromeSync();
        } else {
            InvalidationController.get(mContext).stop();
            if (mAndroidSyncSettings.isMasterSyncEnabled()) {
                // See comment in stop().
                mAndroidSyncSettings.disableChromeSync();
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
     * Sets the SyncNotificationController.
     *
     * This is a temporary method for transferring ownership of SyncNotificationController
     * upstream. Once all of SNC's dependencies are upstreamed, it will be created in the
     * SyncController constructor and this method won't exist.
     */
    public void setSyncNotificationController(SyncNotificationController snc) {
        assert mSyncNotificationController == null;
        mSyncNotificationController = snc;
        mProfileSyncService.addSyncStateChangedListener(mSyncNotificationController);
    }

    /**
     * Returns the SyncNotificationController.
     */
    public SyncNotificationController getSyncNotificationController() {
        return mSyncNotificationController;
    }
}
