// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.document.DocumentUtils;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel.Entry;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/**
 * Interfaces with the ActivityManager to identify Tabs/Tasks that are being tracked by
 * Android's Recents list.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class ActivityDelegate {
    private final Class<?> mRegularClass;
    private final Class<?> mIncognitoClass;

    /**
     * Creates a ActivityDelegate.
     * @param regularClass Class of the regular DocumentActivity.
     * @param incognitoClass Class of the Incognito DocumentActivity.
     */
    public ActivityDelegate(Class<?> regularClass, Class<?> incognitoClass) {
        mRegularClass = regularClass;
        mIncognitoClass = incognitoClass;
    }

    /**
     * Returns whether an Activity is a DocumentActivity.  Assumes the Incognito Activity inherits
     * from the regular Activity.
     * @param activity Activity to check.
     * @return Whether the Activity is a DocumentActivity.
     */
    public boolean isDocumentActivity(Activity activity) {
        return mRegularClass.isInstance(activity);
    }

    /**
     * Checks whether or not the Intent corresponds to an Activity that should be tracked.
     * @param isIncognito Whether or not the TabModel is managing incognito tabs.
     * @param intent Intent used to launch the Activity.
     * @return Whether or not to track the Activity.
     */
    public boolean isValidActivity(boolean isIncognito, Intent intent) {
        if (intent == null) return false;
        String desiredClassName = isIncognito ? mIncognitoClass.getName() : mRegularClass.getName();
        String className = null;
        if (intent.getComponent() == null) {
            Context context = ApplicationStatus.getApplicationContext();
            PackageManager pm = context.getPackageManager();
            ResolveInfo resolveInfo = pm.resolveActivity(intent, 0);
            if (resolveInfo != null) className = resolveInfo.activityInfo.name;
        } else {
            className = intent.getComponent().getClassName();
        }

        return TextUtils.equals(className, desiredClassName);
    }

    /**
     * Get a map of the Chrome tasks displayed by Android's Recents.
     * @param isIncognito Whether or not the TabList is managing incognito tabs.
     */
    public List<Entry> getTasksFromRecents(boolean isIncognito) {
        Context context = ApplicationStatus.getApplicationContext();
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);

        List<Entry> entries = new ArrayList<Entry>();
        for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
            Intent intent = DocumentUtils.getBaseIntentFromTask(task);
            if (!isValidActivity(isIncognito, intent)) continue;

            int tabId = getTabIdFromIntent(intent);
            String initialUrl = getInitialUrlForDocument(intent);
            if (tabId == Tab.INVALID_TAB_ID || initialUrl == null) continue;
            entries.add(new Entry(tabId, initialUrl));
        }
        return entries;
    }

    /**
     * Moves the given task to the front, if it exists.
     * @param isIncognito Whether or not the TabList is managing incognito tabs.
     * @param tabId ID of the tab to move to front.
     */
    public void moveTaskToFront(boolean isIncognito, int tabId) {
        ActivityManager.AppTask task = getTask(isIncognito, tabId);
        if (task != null) task.moveToFront();
    }

    /**
     * Finishes and removes the task.
     * @param isIncognito Whether or not the TabList is managing incognito tabs.
     * @param tabId ID of the tab to move to front.
     */
    public void finishAndRemoveTask(boolean isIncognito, int tabId) {
        ActivityManager.AppTask task = getTask(isIncognito, tabId);
        if (task != null) task.finishAndRemoveTask();
    }

    private ActivityManager.AppTask getTask(boolean isIncognito, int tabId) {
        Context context = ApplicationStatus.getApplicationContext();
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
            Intent intent = DocumentUtils.getBaseIntentFromTask(task);
            int taskId = getTabIdFromIntent(intent);
            if (taskId == tabId && isValidActivity(isIncognito, intent)) return task;
        }
        return null;
    }

    /**
     * Check if the Tab is associated with an Activity that hasn't been destroyed.
     * This catches situations where a DocumentActivity is no longer listed in Android's Recents
     * list, but is not dead yet.
     * @param tabId ID of the Tab to find.
     * @return Whether the tab is still alive.
     */
    public boolean isTabAssociatedWithNonDestroyedActivity(boolean isIncognito, int tabId) {
        List<WeakReference<Activity>> activities = ApplicationStatus.getRunningActivities();
        for (WeakReference<Activity> ref : activities) {
            Activity activity = ref.get();
            if (activity != null
                    && isValidActivity(isIncognito, activity.getIntent())
                    && getTabIdFromIntent(activity.getIntent()) == tabId
                    && !activity.isDestroyed()) {
                return true;
            }
        }
        return false;
    }

    /**
     * Check whether or not the Intent contains an ID for document mode.
     * @param intent Intent to check.
     * @return ID for the document that has the given intent as base intent, or
     *         {@link Tab.INVALID_TAB_ID} if it couldn't be retrieved.
     */
    public static int getTabIdFromIntent(Intent intent) {
        if (intent == null || intent.getData() == null) return Tab.INVALID_TAB_ID;

        Uri data = intent.getData();
        if (!TextUtils.equals(data.getScheme(), UrlConstants.DOCUMENT_SCHEME)) {
            return Tab.INVALID_TAB_ID;
        }

        try {
            return Integer.parseInt(data.getHost());
        } catch (NumberFormatException e) {
            return Tab.INVALID_TAB_ID;
        }
    }

    /**
     * Parse out the URL for a document Intent.
     * @param intent Intent to check.
     * @return The URL that the Intent was fired to display, or null if it couldn't be retrieved.
     */
    public static String getInitialUrlForDocument(Intent intent) {
        if (intent == null || intent.getData() == null) return null;
        Uri data = intent.getData();
        return TextUtils.equals(data.getScheme(), UrlConstants.DOCUMENT_SCHEME)
                ? data.getQuery() : null;
    }
}