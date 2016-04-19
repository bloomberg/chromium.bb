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
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SnippetsLauncher;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.precache.PrecacheController;

/**
 * {@link ChromeBackgroundService} is scheduled through the {@link GcmNetworkManager} when the
 * browser needs to be launched for scheduled tasks, or in response to changing network or power
 * conditions.
 */
public class ChromeBackgroundService extends GcmTaskService {
    private static final String TAG = "BackgroundService";

    @Override
    @VisibleForTesting
    public int onRunTask(final TaskParams params) {
        Log.i(TAG, "Woken up at " + new java.util.Date().toString());
        final Context context = this;
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                switch(params.getTag()) {
                    case BackgroundSyncLauncher.TASK_TAG:
                        handleBackgroundSyncEvent(context);
                        break;

                    case OfflinePageUtils.TASK_TAG:
                        handleOfflinePageBackgroundLoad(context);
                        break;

                    case SnippetsLauncher.TASK_TAG_WIFI_CHARGING:
                    case SnippetsLauncher.TASK_TAG_WIFI:
                    case SnippetsLauncher.TASK_TAG_FALLBACK:
                        handleFetchSnippets(context);
                        break;

                    case SnippetsLauncher.TASK_TAG_RESCHEDULE:
                        handleRescheduleSnippets(context);
                        break;

                    case PrecacheController.PERIODIC_TASK_TAG:
                    case PrecacheController.CONTINUATION_TASK_TAG:
                        handlePrecache(context, params.getTag());
                        break;

                    default:
                        Log.i(TAG, "Unknown task tag " + params.getTag());
                        break;
                }
            }
        });
        return GcmNetworkManager.RESULT_SUCCESS;
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
        fetchSnippets();
    }

    @VisibleForTesting
    protected void fetchSnippets() {
        SnippetsBridge.fetchSnippets();
    }

    private void handleRescheduleSnippets(Context context) {
        if (!SnippetsLauncher.hasInstance()) {
            launchBrowser(context);
        }
        rescheduleSnippets();
    }

    @VisibleForTesting
    protected void rescheduleSnippets() {
        SnippetsBridge.rescheduleFetching();
    }

    private void handlePrecache(Context context, String tag) {
        if (!hasPrecacheInstance()) {
            launchBrowser(context);
        }
        precache(context, tag);
    }

    @VisibleForTesting
    protected boolean hasPrecacheInstance() {
        return PrecacheController.hasInstance();
    }

    @VisibleForTesting
    protected void precache(Context context, String tag) {
        PrecacheController.get(context).precache(tag);
    }

    private void handleOfflinePageBackgroundLoad(Context context) {
        // Gather UMA data to measure how often the user's machine is amenable to background
        // loading when we wake to do a task.
        if (LibraryLoader.isInitialized()) {
            OfflinePageUtils.recordWakeupUMA(context);
        }
    }

    @VisibleForTesting
    @SuppressFBWarnings("DM_EXIT")
    protected void launchBrowser(Context context) {
        Log.i(TAG, "Launching browser");
        try {
            ChromeBrowserInitializer.getInstance(this).handleSynchronousStartup();
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
        PrecacheController.rescheduleTasksOnUpgrade(this);
    }
}
