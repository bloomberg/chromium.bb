// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.app.Activity;
import android.app.IntentService;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.util.Pair;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.util.FeatureUtilities;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Service that handles the action of clicking on the incognito notification.
 */
public class IncognitoNotificationService extends IntentService {

    private static final String TAG = "incognito_notification";

    private static final String ACTION_CLOSE_ALL_INCOGNITO =
            "com.google.android.apps.chrome.incognito.CLOSE_ALL_INCOGNITO";

    @VisibleForTesting
    public static PendingIntent getRemoveAllIncognitoTabsIntent(Context context) {
        Intent intent = new Intent(context, IncognitoNotificationService.class);
        intent.setAction(ACTION_CLOSE_ALL_INCOGNITO);
        return PendingIntent.getService(context, 0, intent, PendingIntent.FLAG_ONE_SHOT);
    }

    /** Empty public constructor needed by Android. */
    public IncognitoNotificationService() {
        super(TAG);
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        boolean isDocumentMode = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return FeatureUtilities.isDocumentMode(IncognitoNotificationService.this);
                    }
                });

        boolean clearedIncognito = true;
        if (isDocumentMode) {
            // TODO(dfalcantara): Delete this when document mode goes away.
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    ChromeApplication.getDocumentTabModelSelector().getModel(true).closeAllTabs();
                }
            });
        } else {
            List<Integer> processedSelectors = closeIncognitoTabsInRunningTabbedActivities();

            for (int i = 0; i < TabWindowManager.MAX_SIMULTANEOUS_SELECTORS; i++) {
                if (processedSelectors.contains(i)) continue;
                clearedIncognito &= deleteIncognitoStateFilesInDirectory(
                        TabPersistentStore.getStateDirectory(this, i));
            }
        }

        // If we failed clearing all of the incognito tabs, then do not dismiss the notification.
        if (!clearedIncognito) return;

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                int incognitoCount = TabWindowManager.getInstance().getIncognitoTabCount();
                assert incognitoCount == 0;

                if (incognitoCount == 0) {
                    IncognitoNotificationManager.dismissIncognitoNotification();
                }
            }
        });
    }

    /**
     * Iterate across the running activities and for any running tabbed mode activities close their
     * incognito tabs.
     *
     * @return The list of window indexes that were processed.
     *
     * @see TabWindowManager#getIndexForWindow(Activity)
     */
    private List<Integer> closeIncognitoTabsInRunningTabbedActivities() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<List<Integer>>() {
                    @Override
                    public List<Integer> call() {
                        List<Integer> selectorIndexes = new ArrayList<>();

                        List<WeakReference<Activity>> runningActivities =
                                ApplicationStatus.getRunningActivities();
                        for (int i = 0; i < runningActivities.size(); i++) {
                            Activity activity = runningActivities.get(i).get();
                            if (activity == null) continue;
                            if (!(activity instanceof ChromeTabbedActivity)) continue;

                            ChromeTabbedActivity tabbedActivity = (ChromeTabbedActivity) activity;
                            if (tabbedActivity.isActivityDestroyed()) continue;

                            tabbedActivity.getTabModelSelector().getModel(true).closeAllTabs();
                            selectorIndexes.add(TabWindowManager.getInstance().getIndexForWindow(
                                    tabbedActivity));
                        }

                        return selectorIndexes;
                    }
                });
    }

    /**
     * @return Whether deleting all the incognito files was successful.
     */
    private boolean deleteIncognitoStateFilesInDirectory(File directory) {
        File[] allTabStates = directory.listFiles();
        if (allTabStates == null) return true;

        boolean deletionSuccessful = true;
        for (int i = 0; i < allTabStates.length; i++) {
            String fileName = allTabStates[i].getName();
            Pair<Integer, Boolean> tabInfo = TabState.parseInfoFromFilename(fileName);
            if (tabInfo == null || !tabInfo.second) continue;
            deletionSuccessful &= allTabStates[i].delete();
        }
        return deletionSuccessful;
    }

}
