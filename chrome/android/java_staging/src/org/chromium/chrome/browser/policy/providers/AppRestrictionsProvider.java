// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy.providers;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;

import org.chromium.chrome.browser.policy.PolicyProvider;

/**
 * Policy provider for Android's App Restriction Schema.
 */
public final class AppRestrictionsProvider extends PolicyProvider {
    private final BroadcastReceiver mAppRestrictionsChangedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            refresh();
        }
    };
    private final UserManager mUserManager;

    /**
     * Register to receive the intent for App Restrictions.
     */
    public AppRestrictionsProvider(Context context) {
        super(context);
        mUserManager = getUserManager();
    }

    @Override
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    protected void startListeningForPolicyChanges() {
        if (!isChangeIntentSupported()) return;
        mContext.registerReceiver(mAppRestrictionsChangedReceiver,
                new IntentFilter(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));
    }

    @Override
    public void refresh() {
        if (!isRestrictionsSupported()) {
            notifySettingsAvailable(new Bundle());
            return;
        }

        new AsyncTask<Void, Void, Bundle>() {
            @Override
            protected Bundle doInBackground(Void... params) {
                return getApplicationRestrictions();
            }

            @Override
            protected void onPostExecute(Bundle result) {
                notifySettingsAvailable(result);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public void destroy() {
        if (isChangeIntentSupported()) {
            mContext.unregisterReceiver(mAppRestrictionsChangedReceiver);
        }
        super.destroy();
    }

    /**
     * getApplicationRestrictions method of UserManger was introduced in JELLY_BEAN_MR2.
     */
    private boolean isRestrictionsSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2;
    }

    /**
     * Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED was introduced in LOLLIPOP.
     */
    private boolean isChangeIntentSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
    }

    /**
     * Wrap access to the Android UserManager to allow being swapped out in environments where it
     * is not available yet.
     */
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private Bundle getApplicationRestrictions() {
        return mUserManager.getApplicationRestrictions(mContext.getPackageName());
    }

    /**
     * Wrap access to the Android UserManager to allow being swapped out in environments where it
     * is not available yet.
     */
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private UserManager getUserManager() {
        if (!isRestrictionsSupported()) return null;
        return (UserManager) mContext.getSystemService(Context.USER_SERVICE);
    }
}
