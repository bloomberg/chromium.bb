// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.preference.PreferenceManager;
import android.support.v4.content.WakefulBroadcastReceiver;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.app.ContentApplication;

/**
 * {@link BackgroundSyncLauncherService} monitors network connectivity and launches
 * the browser when it goes online if the {@link BackgroundSyncLauncher} requested it.
 */
public class BackgroundSyncLauncherService extends IntentService {
    private static final String TAG = "cr.BgSyncLauncher";

    /**
     * Receiver for network connection change broadcasts. If the device is online and the browser
     * should be launched, it starts the BackgroundSyncLauncherService.
     *
     * This class is public so that it can be instantiated by the Android runtime.
     */
    public static class Receiver extends WakefulBroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            // If online, the browser isn't running, and the browser has requested
            // it be launched the next time the device is online, start the browser.
            if (ConnectivityManager.CONNECTIVITY_ACTION.equals(intent.getAction())
                    && isOnline(context) && !BackgroundSyncLauncher.hasInstance()
                    && BackgroundSyncLauncher.shouldLaunchWhenNextOnline(
                               PreferenceManager.getDefaultSharedPreferences(context))) {
                startService(context);
            }
        }

        @VisibleForTesting
        protected void startService(Context context) {
            Intent serviceIntent = new Intent(context, BackgroundSyncLauncherService.class);
            startWakefulService(context, serviceIntent);
        }

        @VisibleForTesting
        protected boolean isOnline(Context context) {
            ConnectivityManager connectivityManager =
                    (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
            NetworkInfo networkInfo = connectivityManager.getActiveNetworkInfo();
            return networkInfo != null && networkInfo.isConnected();
        }
    }

    public BackgroundSyncLauncherService() {
        super("BackgroundSyncLauncherService");
    }

    @Override
    public void onHandleIntent(Intent intent) {
        try {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    onOnline(getApplicationContext());
                }
            });
        } finally {
            WakefulBroadcastReceiver.completeWakefulIntent(intent);
        }
    }

    private void onOnline(Context context) {
        ThreadUtils.assertOnUiThread();

        // Start the browser. The browser's BackgroundSyncManager (for the active profile) will
        // start, check the network, and run any necessary sync events. It runs without a wake lock.
        // TODO(jkarlin): Protect the browser sync event with a wake lock. See crbug.com/486020.
        Log.v(TAG, "Starting Browser after coming online");
        launchBrowser(context);
    }

    @SuppressFBWarnings("DM_EXIT")
    private void launchBrowser(Context context) {
        ContentApplication.initCommandLine(context);
        try {
            BrowserStartupController.get(context, LibraryProcessType.PROCESS_BROWSER)
                    .startBrowserProcessesSync(false);
        } catch (ProcessInitException e) {
            Log.e(TAG, "ProcessInitException while starting the browser process");
            // Since the library failed to initialize nothing in the application
            // can work, so kill the whole application not just the activity.
            System.exit(-1);
        }
    }
}
