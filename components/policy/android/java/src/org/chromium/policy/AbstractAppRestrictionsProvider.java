// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.policy;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Parcel;
import android.preference.PreferenceManager;
import android.util.Base64;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;

import java.util.concurrent.Executor;

/**
 * Retrieves app restrictions and provides them to the parent class as Bundles. Ensures that
 * restrictions can be retrieved early in the application's life cycle by caching previously
 * obtained bundles.
 *
 * Needs to be subclassed to specify how to retrieve the restrictions.
 */
public abstract class AbstractAppRestrictionsProvider extends PolicyProvider {
    private static final String PREFERENCE_KEY = "App Restrictions";

    private static final String TAG = "policy";

    /** {@link Bundle} holding the restrictions to be used during tests. */
    private static Bundle sTestRestrictions = null;

    private final Context mContext;
    private final SharedPreferences mSharedPreferences;
    private final BroadcastReceiver mAppRestrictionsChangedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            refresh();
        }
    };

    private Executor mExecutor = AsyncTask.THREAD_POOL_EXECUTOR;

    /**
     * @param context The application context.
     */
    public AbstractAppRestrictionsProvider(Context context) {
        mContext = context;
        mSharedPreferences = PreferenceManager.getDefaultSharedPreferences(mContext);
    }

    /**
     * @return The restrictions for the provided package name, an empty bundle if they are not
     * available.
     */
    protected abstract Bundle getApplicationRestrictions(String packageName);

    /**
     * @return The intent action to listen to to be notified of restriction changes,
     * {@code null} if it is not supported.
     */
    protected abstract String getRestrictionChangeIntentAction();

    /**
     * Start listening for restrictions changes. Does nothing if this is not supported by the
     * platform.
     */
    @Override
    public void startListeningForPolicyChanges() {
        String changeIntentAction = getRestrictionChangeIntentAction();
        if (changeIntentAction == null) return;
        mContext.registerReceiver(
                mAppRestrictionsChangedReceiver, new IntentFilter(changeIntentAction));
    }

    /**
     * Retrieve the restrictions. {@link #notifySettingsAvailable(Bundle)} will be called as a
     * result.
     */
    @Override
    public void refresh() {
        if (sTestRestrictions != null) {
            notifySettingsAvailable(sTestRestrictions);
            return;
        }

        final Bundle cachedResult = getCachedPolicies();
        if (cachedResult != null) {
            notifySettingsAvailable(cachedResult);
        }

        new AsyncTask<Void, Void, Bundle>() {
            @Override
            protected Bundle doInBackground(Void... params) {
                long startTime = System.currentTimeMillis();
                Bundle bundle = getApplicationRestrictions(mContext.getPackageName());
                recordStartTimeHistogram(startTime);
                return bundle;
            }

            @Override
            protected void onPostExecute(Bundle result) {
                cachePolicies(result);
                notifySettingsAvailable(result);
            }
        }.executeOnExecutor(mExecutor);
    }

    @Override
    public void destroy() {
        stopListening();
        super.destroy();
    }

    /**
     * Stop listening for restrictions changes. Does nothing if this is not supported by the
     * platform.
     */
    public void stopListening() {
        if (getRestrictionChangeIntentAction() != null) {
            mContext.unregisterReceiver(mAppRestrictionsChangedReceiver);
        }
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
        Bundle result;
        try {
            result = p.readBundle();
        } catch (IllegalStateException e) {
            result = null;
        }
        recordCacheLoadResultHistogram(result != null);
        return result;
    }

    // Extracted to allow stubbing, since it calls a static that can't easily be stubbed
    @VisibleForTesting
    protected void recordCacheLoadResultHistogram(final boolean success) {
        RecordHistogram.recordBooleanHistogram(
                "Enterprise.AppRestrictionsCacheLoad", success);
    }

    // Extracted to allow stubbing, since it calls a static that can't easily be stubbed
    @VisibleForTesting
    protected void recordStartTimeHistogram(long startTime) {
        // TODO(aberent): Re-implement once we understand why the previous implementation was giving
        // random crashes (https://crbug.com/535043)
    }
    /**
     * @param testExecutor - The executor to use for this class's AsyncTasks.
     */
    @VisibleForTesting
    void setTaskExecutor(Executor testExecutor) {
        mExecutor = testExecutor;
    }

    /**
     * Restrictions to be used during tests. Subsequent attempts to retrieve the restrictions will
     * return the provided bundle instead.
     *
     * Chrome and WebView tests are set up to use annotations for policy testing and reset the
     * restrictions to an empty bundle if nothing is specified. To stop using a test bundle,
     * provide {@code null} as value instead.
     */
    @VisibleForTesting
    public static void setTestRestrictions(Bundle policies) {
        Log.d(TAG, "Test Restrictions: %s", policies.keySet().toArray());
        sTestRestrictions = policies;
    }
}
