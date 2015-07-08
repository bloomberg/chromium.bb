// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.policy;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;

/**
 * Retrieves app restrictions and provides them to a {@link Delegate} object as Bundles.
 * Retrieving the restrictions is done asynchronously.
 */
public class AppRestrictionsProvider {
    /** Delegate to notify when restrictions have been received. */
    public interface Delegate {
        /** Called when new restrictions are available. */
        public void notifyNewAppRestrictionsAvailable(Bundle newAppRestrictions);
    }

    private final UserManager mUserManager;
    private final Context mContext;
    private final Delegate mDelegate;
    private final BroadcastReceiver mAppRestrictionsChangedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            refreshRestrictions();
        }
    };

    /**
     * @param context The application context.
     * @param appRestrictionsProviderDelegate Object to be notified when new restrictions
     *  are available.
     */
    public AppRestrictionsProvider(Context context, Delegate appRestrictionsProviderDelegate) {
        mContext = context;
        mDelegate = appRestrictionsProviderDelegate;
        mUserManager = getUserManager();
    }

    /**
     * Start listening for restrictions changes. Does nothing if this is not supported by the
     * platform.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void startListening() {
        if (!isChangeIntentSupported()) return;
        mContext.registerReceiver(mAppRestrictionsChangedReceiver,
                new IntentFilter(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));
    }

    /**
     * Asynchronously retrieve the restrictions.
     * {@link Delegate#notifyNewAppRestrictionsAvailable(Bundle)} will be called as a result.
     */
    public void refreshRestrictions() {
        if (!isRestrictionsSupported()) {
            mDelegate.notifyNewAppRestrictionsAvailable(new Bundle());
            return;
        }

        new AsyncTask<Void, Void, Bundle>() {
            @Override
            protected Bundle doInBackground(Void... params) {
                return getApplicationRestrictions();
            }

            @Override
            protected void onPostExecute(Bundle result) {
                mDelegate.notifyNewAppRestrictionsAvailable(result);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Stop listening for restrictions changes. Does nothing if this is not supported by the
     * platform.
     */
    public void stopListening() {
        if (isChangeIntentSupported()) {
            mContext.unregisterReceiver(mAppRestrictionsChangedReceiver);
        }
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
