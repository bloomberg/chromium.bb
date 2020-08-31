// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;

import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;

/**
 * Requests {@link WebappDataStorage} during deferred startup. For WebAPKs only, creates
 * {@link WebappDataStorage} if the WebAPK is not registered. Runs tasks once the
 * {@link WebappDataStorage} has been fetched (and perhaps also created).
 */
@ActivityScope
public class WebappDeferredStartupWithStorageHandler {
    interface Task {
        /**
         * Called to run task.
         * @param storage Null if there is no {@link WebappDataStorage} registered for the webapp
         *                and a new entry was not created.
         * @param didCreateStorage Whether a new {@link WebappDataStorage} entry was created.
         */
        void run(@Nullable WebappDataStorage storage, boolean didCreateStorage);
    }

    private final ChromeActivity<?> mActivity;
    private final @Nullable String mWebappId;
    private final boolean mIsWebApk;
    private final List<Task> mDeferredWithStorageTasks = new ArrayList<>();

    @Inject
    public WebappDeferredStartupWithStorageHandler(
            ChromeActivity<?> activity, BrowserServicesIntentDataProvider intentDataProvider) {
        mActivity = activity;

        WebappExtras webappExtras = intentDataProvider.getWebappExtras();
        mWebappId = (webappExtras != null) ? webappExtras.id : null;
        mIsWebApk = intentDataProvider.isWebApkActivity();
    }

    /**
     * Invoked to add deferred startup task to queue.
     */
    public void initDeferredStartupForActivity() {
        DeferredStartupHandler.getInstance().addDeferredTask(() -> { runDeferredTask(); });
    }

    public void addTask(Task task) {
        mDeferredWithStorageTasks.add(task);
    }

    public void addTaskToFront(Task task) {
        mDeferredWithStorageTasks.add(0, task);
    }

    private void runDeferredTask() {
        if (mActivity.isActivityFinishingOrDestroyed()) return;

        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(mWebappId);
        if (storage != null || !mIsWebApk) {
            runTasks(storage, false /* didCreateStorage */);
            return;
        }

        WebappRegistry.getInstance().register(
                mWebappId, new WebappRegistry.FetchWebappDataStorageCallback() {
                    @Override
                    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                        runTasks(storage, true /* didCreateStorage */);
                    }
                });
    }

    public void runTasks(@Nullable WebappDataStorage storage, boolean didCreateStorage) {
        for (Task task : mDeferredWithStorageTasks) {
            task.run(storage, didCreateStorage);
        }
        mDeferredWithStorageTasks.clear();
    }
}
