// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.policy;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Parcel;
import android.os.UserManager;
import android.preference.PreferenceManager;
import android.util.Base64;

import org.chromium.base.metrics.RecordHistogram;

import java.util.concurrent.TimeUnit;

/**
 * Retrieves app restrictions and provides them to a {@link Delegate} object as Bundles. Retrieving
 * the restrictions is done asynchronously.
 */
public class AppRestrictionsProvider {
    /** Delegate to notify when restrictions have been received. */
    public interface Delegate {
        /** Called when new restrictions are available. */
        public void notifyNewAppRestrictionsAvailable(Bundle newAppRestrictions);
    }

    private static final String PREFERENCE_KEY = "App Restrictions";

    private final UserManager mUserManager;
    private final Context mContext;
    private final Delegate mDelegate;
    private final SharedPreferences mSharedPreferences;
    private final BroadcastReceiver mAppRestrictionsChangedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            refreshRestrictions();
        }
    };

    /**
     * @param context The application context.
     * @param appRestrictionsProviderDelegate Object to be notified when new restrictions are
     *            available.
     */
    public AppRestrictionsProvider(Context context, Delegate appRestrictionsProviderDelegate) {
        mContext = context;
        mDelegate = appRestrictionsProviderDelegate;
        mUserManager = getUserManager();
        mSharedPreferences = PreferenceManager.getDefaultSharedPreferences(mContext);
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
        final Bundle cachedResult = getCachedPolicies();
        if (cachedResult != null) {
            mDelegate.notifyNewAppRestrictionsAvailable(cachedResult);
        }

        new AsyncTask<Void, Void, Bundle>() {
            @Override
            protected Bundle doInBackground(Void... params) {
                long startTime = System.currentTimeMillis();
                Bundle bundle = getApplicationRestrictions();
                RecordHistogram.recordTimesHistogram("Enterprise.AppRestrictionLoadTime",
                        System.currentTimeMillis() - startTime, TimeUnit.MILLISECONDS);
                return bundle;
            }

            @Override
            protected void onPostExecute(Bundle result) {
                cachePolicies(result);
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
     * Wrap access to the Android UserManager to allow being swapped out in environments where it is
     * not available yet.
     */
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private Bundle getApplicationRestrictions() {
        return mUserManager.getApplicationRestrictions(mContext.getPackageName());
    }

    /**
     * Wrap access to the Android UserManager to allow being swapped out in environments where it is
     * not available yet.
     */
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private UserManager getUserManager() {
        if (!isRestrictionsSupported()) return null;
        return (UserManager) mContext.getSystemService(Context.USER_SERVICE);
    }

    private void cachePolicies(Bundle policies) {
        Parcel p = Parcel.obtain();
        p.writeBundle(policies);
        byte bytes[] = p.marshall();
        String s = Base64.encodeToString(bytes, 0);
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putString(PREFERENCE_KEY, s);
        ed.apply();
    }

    private Bundle getCachedPolicies() {
        String s = mSharedPreferences.getString(PREFERENCE_KEY, null);
        if (s == null) {
            return null;
        }
        byte bytes[] = Base64.decode(s, 0);
        Parcel p = Parcel.obtain();
        // Unmarshalling the parcel is, in theory, unsafe if the Android version or API version has
        // changed, but the worst that is likely to happen is that the bundle comes back empty, and
        // this will be corrected once the Android returns the real App Restrictions.
        p.unmarshall(bytes, 0, bytes.length);
        p.setDataPosition(0);
        Bundle result = p.readBundle();
        try {
            result = p.readBundle();
        } catch (IllegalStateException e) {
            result = null;
        }
        RecordHistogram.recordBooleanHistogram(
                "Enterprise.AppRestrictionsCacheLoad", result != null);
        return result;
    }
}
