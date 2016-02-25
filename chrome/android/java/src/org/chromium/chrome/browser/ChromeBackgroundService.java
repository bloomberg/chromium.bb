// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.GcmTaskService;
import com.google.android.gms.gcm.TaskParams;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.ntp.snippets.SnippetsController;
import org.chromium.chrome.browser.ntp.snippets.SnippetsLauncher;
import org.chromium.content.app.ContentApplication;
import org.chromium.content.browser.BrowserStartupController;

/**
 * {@link ChromeBackgroundService} is scheduled through the {@link GcmNetworkManager} when the
 * browser needs to be launched for scheduled tasks, or in response to changing network or power
 * conditions.
 */
public class ChromeBackgroundService extends GcmTaskService {
    private static final String TAG = "BackgroundService";

    @Override
    public int onRunTask(TaskParams params) {
        Log.i(TAG, "Woken up at " + new java.util.Date().toString());
        handleRunTask(params.getTag());
        return GcmNetworkManager.RESULT_SUCCESS;
    }

    @VisibleForTesting
    public void handleRunTask(final String tag) {
        final Context context = this;
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                switch(tag) {
                    case BackgroundSyncLauncher.TASK_TAG:
                        handleBackgroundSyncEvent(context);
                        break;

                    case SnippetsLauncher.TASK_TAG:
                        handleFetchSnippets(context);
                        break;

                    default:
                        Log.i(TAG, "Unknown task tag " + tag);
                        break;
                }
            }
        });
    }

    private void handleBackgroundSyncEvent(Context context) {
        if (!BackgroundSyncLauncher.hasInstance()) {
            // Start the browser. The browser's BackgroundSyncManager (for the active profile) will
            // start, check the network, and run any necessary sync events. This task runs with a
            // wake lock, but has a three minute timeout, so we need to start the browser in its
            // own task.
            // TODO(jkarlin): Protect the browser sync event with a wake lock.
            // See crbug.com/486020.
            launchBrowser(context);
        }
    }

    private void handleFetchSnippets(Context context) {
        if (!SnippetsLauncher.hasInstance()) {
            launchBrowser(context);
        }
        SnippetsController.get(context).fetchSnippets(true);
    }

    @VisibleForTesting
    @SuppressFBWarnings("DM_EXIT")
    protected void launchBrowser(Context context) {
        Log.i(TAG, "Launching browser");
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

    @Override
    public void onInitializeTasks() {
        BackgroundSyncLauncher.rescheduleTasksOnUpgrade(this);
    }
}
