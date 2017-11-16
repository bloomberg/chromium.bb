// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.variations;

import android.annotation.TargetApi;
import android.app.job.JobParameters;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import org.chromium.android_webview.variations.AwVariationsUtils.SeedPreference;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.IOException;
import java.util.ArrayList;

/**
 * AwVariationsSeedFetchService is a Job Service to fetch test seed data which is used by Variations
 * to enable AB testing experiments in the native code. The fetched data is stored in WebView's data
 * directory. The tryLock mechanism is just to make sure that
 * no seed fetch job will be scheduled when there is one still running. This is a prototype of the
 * Variations Seed Fetch Service which is one part of the work of adding Variations to Android
 * WebView.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP) // JobService requires API level 21.
public class AwVariationsSeedFetchService extends JobService {
    private static final String TAG = "AwVartnsSeedFetchSvc";

    private FetchVariationsSeedTask mFetchVariationsSeedTask;

    @Override
    public boolean onStartJob(JobParameters params) {
        // Ensure we can use ContextUtils later on.
        ContextUtils.initApplicationContext(this.getApplicationContext());
        mFetchVariationsSeedTask = new FetchVariationsSeedTask(params);
        mFetchVariationsSeedTask.execute();
        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        // This method is called by the JobScheduler to stop a job before it has finished. If the
        // situation happens, we should cancel the running task.
        if (mFetchVariationsSeedTask != null) {
            mFetchVariationsSeedTask.cancel(true);
        }

        // TODO(paulmiller): Determine if we actually don't want to reschedule the job in the event
        // it is stopped.
        return false;
    }

    private class FetchVariationsSeedTask extends AsyncTask<Void, Void, Void> {
        private JobParameters mJobParams;

        FetchVariationsSeedTask(JobParameters params) {
            mJobParams = params;
        }

        @Override
        protected Void doInBackground(Void... params) {
            fetchVariationsSeed();
            return null;
        }

        @Override
        protected void onPostExecute(Void success) {
            jobFinished(mJobParams, false /* false -> don't reschedule */);
        }
    }

    private void fetchVariationsSeed() {
        assert !ThreadUtils.runningOnUiThread();
        try {
            // TODO(paulmiller): Addd milestone and channel information to seed request.
            // crbug.com/784905
            SeedInfo seedInfo = VariationsSeedFetcher.get().downloadContent(
                    VariationsSeedFetcher.VariationsPlatform.ANDROID_WEBVIEW, "", "", "");
            transferFetchedSeed(seedInfo);
        } catch (IOException e) {
            // Exceptions are handled and logged in the downloadContent method, so we don't
            // need any exception handling here. The only reason we need a catch-statement here
            // is because those exceptions are re-thrown from downloadContent to skip the
            // normal logic flow within that method.
        }
    }

    private static class SeedTransferConnection implements ServiceConnection {
        private SeedInfo mSeedInfo;

        SeedTransferConnection(SeedInfo seedInfo) {
            mSeedInfo = seedInfo;
        }

        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            Bundle bundle = new Bundle();
            bundle.putByteArray(AwVariationsUtils.KEY_SEED_DATA, mSeedInfo.seedData);
            ArrayList<String> list = new SeedPreference(mSeedInfo).toArrayList();
            bundle.putStringArrayList(AwVariationsUtils.KEY_SEED_PREF, list);

            // Directly set members of AwVariationsConfigurationService to prevent a malicious app
            // from sending fake seed data.
            AwVariationsConfigurationService.sMsgFromSeedFetchService = Message.obtain();
            AwVariationsConfigurationService.sMsgFromSeedFetchService.setData(bundle);

            Message msg = Message.obtain();
            msg.what = AwVariationsUtils.MSG_SEED_TRANSFER;
            try {
                new Messenger(service).send(msg);
            } catch (RemoteException e) {
                Log.e(TAG, "Failed to send seed data to AwVariationsConfigurationService. " + e);
            }

            // After the data has been sent the AwVariationsConfigurationService can unbind from the
            // current service.
            ContextUtils.getApplicationContext().unbindService(this);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    private void transferFetchedSeed(SeedInfo seedInfo) {
        Intent intent = new Intent(this, AwVariationsConfigurationService.class);
        ServiceConnection connection = new SeedTransferConnection(seedInfo);

        Context context = ContextUtils.getApplicationContext();
        if (!context.bindService(intent, connection, Context.BIND_AUTO_CREATE)) {
            Log.w(TAG, "Could not bind to AwVariationsConfigurationService. " + intent);
        }
    }
}
