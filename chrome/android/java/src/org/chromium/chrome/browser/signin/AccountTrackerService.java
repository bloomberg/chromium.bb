// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.os.AsyncTask;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.sync.signin.AccountManagerHelper;

import java.util.List;

/**
* Android wrapper of AccountTrackerService which provides access from the java layer.
* It offers the capability of fetching and seeding system accounts into AccountTrackerService in C++
* layer, and notifies observers when it is complete.
*/
public class AccountTrackerService {
    private static final String TAG = "cr.AccountService";
    private static AccountTrackerService sAccountTrackerService;
    private static AccountIdProvider sAccountIdProvider;
    private SystemAccountsSeedingStatus mSystemAccountsSeedingStatus =
            SystemAccountsSeedingStatus.SEEDING_NOT_STARTED;
    private boolean mForceRefresh;
    private boolean mSyncForceRefreshedForTest;

    private final Context mContext;
    private final long mNativeAccountTrackerService;

    private enum SystemAccountsSeedingStatus {
        SEEDING_NOT_STARTED,
        SEEDING_IN_PROGRESS,
        SEEDING_DONE
    }

    /**
    * Classes that want to listen for system accounts fetching and seeding should implement
    * this interface and register with {@link #addSystemAccountsSeededListener}.
    */
    public interface OnSystemAccountsSeededListener {
        // Called at the end of seedSystemAccounts().
        void onSystemAccountsSeedingComplete();
        // Called at the beginning of system accounts being force refreshed.
        void onSystemAccountsForceRefreshed();
    }

    private final ObserverList<OnSystemAccountsSeededListener> mSystemAccountsSeedingObservers =
            new ObserverList<>();

    public static AccountTrackerService get(Context context) {
        ThreadUtils.assertOnUiThread();
        if (sAccountTrackerService == null) {
            sAccountTrackerService = new AccountTrackerService(context);
            sAccountIdProvider = AccountIdProvider.getInstance();
        }
        return sAccountTrackerService;
    }

    private AccountTrackerService(Context context) {
        mContext = context;
        mNativeAccountTrackerService = nativeInit();
    }

    /**
    * Check whether system accounts have been seeded into AccountTrackerService in C++ layer.
    */
    public boolean isSystemAccountsSeeded() {
        ThreadUtils.assertOnUiThread();
        if (mSystemAccountsSeedingStatus == SystemAccountsSeedingStatus.SEEDING_DONE) {
            return true;
        }

        if (mSystemAccountsSeedingStatus == SystemAccountsSeedingStatus.SEEDING_NOT_STARTED) {
            seedSystemAccounts();
        }

        return false;
    }

    /**
    * Register an |observer| to observe system accounts seeding status.
    */
    public void addSystemAccountsSeededListener(OnSystemAccountsSeededListener observer) {
        ThreadUtils.assertOnUiThread();
        mSystemAccountsSeedingObservers.addObserver(observer);
        if (mSystemAccountsSeedingStatus == SystemAccountsSeedingStatus.SEEDING_DONE) {
            observer.onSystemAccountsSeedingComplete();
        }
    }

    private void seedSystemAccounts() {
        ThreadUtils.assertOnUiThread();
        mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_IN_PROGRESS;
        mForceRefresh = false;
        mSyncForceRefreshedForTest = false;
        AccountManagerHelper accountManagerHelper = AccountManagerHelper.get(mContext);
        List<String> accountNamesList = accountManagerHelper.getGoogleAccountNames();
        final String[] accountNames = accountNamesList.toArray(new String[accountNamesList.size()]);
        new AsyncTask<Void, Void, String[]>() {
            @Override
            public String[] doInBackground(Void... params) {
                Log.d(TAG, "Getting id/email mapping");
                String[] accountIds = new String[accountNames.length];
                for (int i = 0; i < accountIds.length; ++i) {
                    accountIds[i] = sAccountIdProvider.getAccountId(mContext, accountNames[i]);
                }
                return accountIds;
            }
            @Override
            public void onPostExecute(String[] accountIds) {
                if (mSyncForceRefreshedForTest) return;
                if (mForceRefresh) {
                    seedSystemAccounts();
                    return;
                }
                if (areAccountIdsValid(accountIds)) {
                    nativeSeedAccountsInfo(mNativeAccountTrackerService, accountIds, accountNames);
                    mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_DONE;
                    notifyObserversOnSeedingComplete();
                } else {
                    Log.w(TAG, "Invalid mapping of id/email");
                    if (sAccountIdProvider.canBeUsed(mContext, null)) {
                        seedSystemAccounts();
                        return;
                    }
                    mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_NOT_STARTED;
                    notifyObserversOnForceRefreshed();
                }
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private boolean areAccountIdsValid(String[] accountIds) {
        for (int i = 0; i < accountIds.length; ++i) {
            if (accountIds[i] == null) return false;
        }
        return true;
    }

    private void notifyObserversOnSeedingComplete() {
        for (OnSystemAccountsSeededListener observer : mSystemAccountsSeedingObservers) {
            observer.onSystemAccountsSeedingComplete();
        }
    }

    /**
    * Seed system accounts into AccountTrackerService synchronously for test purpose.
    */
    @VisibleForTesting
    public void syncForceRefreshForTest(String[] accountIds, String[] accountNames) {
        ThreadUtils.assertOnUiThread();
        mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_IN_PROGRESS;
        mForceRefresh = false;
        mSyncForceRefreshedForTest = true;
        nativeSeedAccountsInfo(mNativeAccountTrackerService, accountIds, accountNames);
        mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_DONE;
    }

    /**
    * Force AccountTrackerService refresh/update systems accounts.
    */
    public void forceRefresh() {
        ThreadUtils.assertOnUiThread();
        mForceRefresh = true;
        notifyObserversOnForceRefreshed();
        if (mSystemAccountsSeedingStatus != SystemAccountsSeedingStatus.SEEDING_IN_PROGRESS) {
            seedSystemAccounts();
        }
    }

    private void notifyObserversOnForceRefreshed() {
        for (OnSystemAccountsSeededListener observer : mSystemAccountsSeedingObservers) {
            observer.onSystemAccountsForceRefreshed();
        }
    }

    // Native methods.
    private native long nativeInit();
    private native void nativeSeedAccountsInfo(
            long nativeAccountTrackerServiceAndroid, String[] gaiaIds, String[] accountNames);
}
