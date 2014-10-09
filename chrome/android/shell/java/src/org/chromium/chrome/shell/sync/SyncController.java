// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell.sync;

import android.accounts.Account;
import android.app.Activity;
import android.app.FragmentManager;
import android.content.Context;
import android.util.Log;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.invalidation.InvalidationController;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInFlowObserver;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.sync.notifier.SyncStatusHelper;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;

/**
 * A helper class for managing sync state for the ChromeShell.
 *
 * Builds on top of the ProfileSyncService (which manages Chrome's sync engine's state) and mimics
 * the minimum additional functionality needed to fully enable sync for Chrome on Android.
 */
public class SyncController implements ProfileSyncService.SyncStateChangedListener,
        SyncStatusHelper.SyncSettingsChangedObserver {
    private static final String TAG = "SyncController";

    private static final String SESSIONS_UUID_PREF_KEY = "chromium.sync.sessions.id";

    private static SyncController sInstance;

    private final Context mContext;
    private final ChromeSigninController mChromeSigninController;
    private final SyncStatusHelper mSyncStatusHelper;
    private final ProfileSyncService mProfileSyncService;

    private SyncController(Context context) {
        mContext = context;
        mChromeSigninController = ChromeSigninController.get(mContext);
        mSyncStatusHelper = SyncStatusHelper.get(context);
        mSyncStatusHelper.registerSyncSettingsChangedObserver(this);
        mProfileSyncService = ProfileSyncService.get(mContext);
        mProfileSyncService.addSyncStateChangedListener(this);

        setupSessionSyncId();
        mChromeSigninController.ensureGcmIsInitialized();
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
     * Open a dialog that gives the user the option to sign in from a list of available accounts.
     *
     * @param fragmentManager the FragmentManager.
     */
    public static void openSigninDialog(FragmentManager fragmentManager) {
        AccountChooserFragment chooserFragment = new AccountChooserFragment();
        chooserFragment.show(fragmentManager, null);
    }

    /**
     * Open a dialog that gives the user the option to sign out.
     *
     * @param fragmentManager the FragmentManager.
     */
    public static void openSignOutDialog(FragmentManager fragmentManager) {
        SignoutFragment signoutFragment = new SignoutFragment();
        signoutFragment.show(fragmentManager, null);
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

    public void onStart() {
        refreshSyncState();
    }

    private void setupSessionSyncId() {
        // Ensure that sync uses the correct UniqueIdentificationGenerator, but do not force the
        // registration, in case a test case has already overridden it.
        UuidBasedUniqueIdentificationGenerator generator =
                new UuidBasedUniqueIdentificationGenerator(mContext, SESSIONS_UUID_PREF_KEY);
        UniqueIdentificationGeneratorFactory.registerGenerator(
                UuidBasedUniqueIdentificationGenerator.GENERATOR_ID, generator, false);
        // Since we do not override the UniqueIdentificationGenerator, we get it from the factory,
        // instead of using the instance we just created.
        mProfileSyncService.setSessionsId(UniqueIdentificationGeneratorFactory
                .getInstance(UuidBasedUniqueIdentificationGenerator.GENERATOR_ID));
    }

    private void refreshSyncState() {
        if (mSyncStatusHelper.isSyncEnabled())
            start();
        else
            stop();
    }

    private void start() {
        ThreadUtils.assertOnUiThread();
        if (mSyncStatusHelper.isMasterSyncAutomaticallyEnabled()) {
            Log.d(TAG, "Enabling sync");
            Account account = mChromeSigninController.getSignedInUser();
            InvalidationController.get(mContext).start();
            mProfileSyncService.enableSync();
            mSyncStatusHelper.enableAndroidSync(account);
        }
    }

    /**
     * Stops Sync if a user is currently signed in.
     */
    public void stop() {
        ThreadUtils.assertOnUiThread();
        if (mChromeSigninController.isSignedIn()) {
            Log.d(TAG, "Disabling sync");
            Account account = mChromeSigninController.getSignedInUser();
            InvalidationController.get(mContext).stop();
            mProfileSyncService.disableSync();
            mSyncStatusHelper.disableAndroidSync(account);
        }
    }

    /**
     * From {@link ProfileSyncService.SyncStateChangedListener}.
     */
    @Override
    public void syncStateChanged() {
        ThreadUtils.assertOnUiThread();
        // If sync has been disabled from the dashboard, we must disable it.
        Account account = mChromeSigninController.getSignedInUser();
        boolean isSyncSuppressStart = mProfileSyncService.isStartSuppressed();
        boolean isSyncEnabled = mSyncStatusHelper.isSyncEnabled(account);
        if (account != null && isSyncSuppressStart && isSyncEnabled)
            stop();
    }

    /**
     * From {@link SyncStatusHelper.SyncSettingsChangedObserver}.
     */
    @Override
    public void syncSettingsChanged() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                refreshSyncState();
            }
        });
    }
}
