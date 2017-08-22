// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;

import java.util.HashSet;
import java.util.Set;

class BackgroundTaskSchedulerUma {
    // BackgroundTaskId defined in tools/metrics/histograms/enums.xml
    static final int BACKGROUND_TASK_TEST = 0;
    static final int BACKGROUND_TASK_OMAHA = 1;
    static final int BACKGROUND_TASK_GCM = 2;
    static final int BACKGROUND_TASK_NOTIFICATIONS = 3;
    static final int BACKGROUND_TASK_WEBVIEW_MINIDUMP = 4;
    static final int BACKGROUND_TASK_CHROME_MINIDUMP = 5;
    static final int BACKGROUND_TASK_OFFLINE_PAGES = 6;
    static final int BACKGROUND_TASK_OFFLINE_PREFETCH = 7;
    static final int BACKGROUND_TASK_DOWNLOAD_SERVICE = 8;
    static final int BACKGROUND_TASK_DOWNLOAD_CLEANUP = 9;
    static final int BACKGROUND_TASK_WEBVIEW_VARIATIONS = 10;
    // Keep this one at the end and increment appropriately when adding new tasks.
    static final int BACKGROUND_TASK_COUNT = 11;

    static final String KEY_CACHED_UMA = "bts_cached_uma";

    private static BackgroundTaskSchedulerUma sInstance;

    private static class CachedUmaEntry {
        private static final String SEPARATOR = ":";
        private String mEvent;
        private int mValue;
        private int mCount;

        /**
         * Parses a cached UMA entry from a string.
         *
         * @param entry A serialized entry from preferences store.
         * @return A parsed CachedUmaEntry object, or <c>null</c> if parsing failed.
         */
        public static CachedUmaEntry parseEntry(String entry) {
            if (entry == null) return null;

            String[] entryParts = entry.split(SEPARATOR);
            if (entryParts.length != 3 || entryParts[0].isEmpty() || entryParts[1].isEmpty()
                    || entryParts[2].isEmpty()) {
                return null;
            }
            int value = -1;
            int count = -1;
            try {
                value = Integer.parseInt(entryParts[1]);
                count = Integer.parseInt(entryParts[2]);
            } catch (NumberFormatException e) {
                return null;
            }
            return new CachedUmaEntry(entryParts[0], value, count);
        }

        /** Returns a string for partial matching of the prefs entry. */
        public static String getStringForPartialMatching(String event, int value) {
            return event + SEPARATOR + value + SEPARATOR;
        }

        public CachedUmaEntry(String event, int value, int count) {
            mEvent = event;
            mValue = value;
            mCount = count;
        }

        /** Converts cached UMA entry to a string in format: EVENT:VALUE:COUNT. */
        public String toString() {
            return mEvent + SEPARATOR + mValue + SEPARATOR + mCount;
        }

        /** Gets the name of the event (UMA). */
        public String getEvent() {
            return mEvent;
        }

        /** Gets the value of the event (concrete value of the enum). */
        public int getValue() {
            return mValue;
        }

        /** Gets the count of events that happened. */
        public int getCount() {
            return mCount;
        }

        /** Increments the count of the event. */
        public void increment() {
            mCount++;
        }
    }

    public static BackgroundTaskSchedulerUma getInstance() {
        if (sInstance == null) {
            sInstance = new BackgroundTaskSchedulerUma();
        }
        return sInstance;
    }

    @VisibleForTesting
    public static void setInstanceForTesting(BackgroundTaskSchedulerUma instance) {
        sInstance = instance;
    }

    /** Reports metrics for task scheduling and whether it was successful. */
    public void reportTaskScheduled(int taskId, boolean success) {
        if (success) {
            cacheEvent("Android.BackgroundTaskScheduler.TaskScheduled.Success",
                    toUmaEnumValueFromTaskId(taskId));
        } else {
            cacheEvent("Android.BackgroundTaskScheduler.TaskScheduled.Failure",
                    toUmaEnumValueFromTaskId(taskId));
        }
    }

    /** Reports metrics for task canceling. */
    public void reportTaskCanceled(int taskId) {
        cacheEvent(
                "Android.BackgroundTaskScheduler.TaskCanceled", toUmaEnumValueFromTaskId(taskId));
    }

    /** Reports metrics for starting a task. */
    public void reportTaskStarted(int taskId) {
        cacheEvent("Android.BackgroundTaskScheduler.TaskStarted", toUmaEnumValueFromTaskId(taskId));
    }

    /** Reports metrics for stopping a task. */
    public void reportTaskStopped(int taskId) {
        cacheEvent("Android.BackgroundTaskScheduler.TaskStopped", toUmaEnumValueFromTaskId(taskId));
    }

    /** Method that actually invokes histogram recording. Extracted for testing. */
    @VisibleForTesting
    void recordEnumeratedHistogram(String histogram, int value, int maxCount) {
        RecordHistogram.recordEnumeratedHistogram(histogram, value, maxCount);
    }

    /** Records histograms for cached stats. Should only be called when native is initialized. */
    public void flushStats() {
        assertNativeIsLoaded();
        ThreadUtils.assertOnUiThread();

        Set<String> cachedUmaStrings = getCachedUmaEntries(ContextUtils.getAppSharedPreferences());

        for (String cachedUmaString : cachedUmaStrings) {
            CachedUmaEntry entry = CachedUmaEntry.parseEntry(cachedUmaString);
            if (entry == null) continue;
            for (int i = 0; i < entry.getCount(); i++) {
                recordEnumeratedHistogram(
                        entry.getEvent(), entry.getValue(), BACKGROUND_TASK_COUNT);
            }
        }

        // Once all metrics are reported, we can simply remove the shared preference key.
        removeCachedStats();
    }

    /** Removes all of the cached stats without reporting. */
    public void removeCachedStats() {
        ThreadUtils.assertOnUiThread();
        ContextUtils.getAppSharedPreferences().edit().remove(KEY_CACHED_UMA).apply();
    }

    /** Caches the event to be reported through UMA in shared preferences. */
    @VisibleForTesting
    void cacheEvent(String event, int value) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> cachedUmaStrings = getCachedUmaEntries(prefs);
        String partialMatch = CachedUmaEntry.getStringForPartialMatching(event, value);

        String existingEntry = null;
        for (String cachedUmaString : cachedUmaStrings) {
            if (cachedUmaString.startsWith(partialMatch)) {
                existingEntry = cachedUmaString;
                break;
            }
        }

        Set<String> setToWriteBack = new HashSet<>(cachedUmaStrings);
        CachedUmaEntry entry = null;
        if (existingEntry != null) {
            entry = CachedUmaEntry.parseEntry(existingEntry);
            if (entry == null) {
                entry = new CachedUmaEntry(event, value, 1);
            }
            setToWriteBack.remove(existingEntry);
            entry.increment();
        } else {
            entry = new CachedUmaEntry(event, value, 1);
        }

        setToWriteBack.add(entry.toString());
        updateCachedUma(prefs, setToWriteBack);
    }

    @VisibleForTesting
    static int toUmaEnumValueFromTaskId(int taskId) {
        switch (taskId) {
            case TaskIds.TEST:
                return BACKGROUND_TASK_TEST;
            case TaskIds.OMAHA_JOB_ID:
                return BACKGROUND_TASK_OMAHA;
            case TaskIds.GCM_BACKGROUND_TASK_JOB_ID:
                return BACKGROUND_TASK_GCM;
            case TaskIds.NOTIFICATION_SERVICE_JOB_ID:
                return BACKGROUND_TASK_NOTIFICATIONS;
            case TaskIds.WEBVIEW_MINIDUMP_UPLOADING_JOB_ID:
                return BACKGROUND_TASK_WEBVIEW_MINIDUMP;
            case TaskIds.CHROME_MINIDUMP_UPLOADING_JOB_ID:
                return BACKGROUND_TASK_CHROME_MINIDUMP;
            case TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID:
                return BACKGROUND_TASK_OFFLINE_PAGES;
            case TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID:
                return BACKGROUND_TASK_OFFLINE_PREFETCH;
            case TaskIds.DOWNLOAD_SERVICE_JOB_ID:
                return BACKGROUND_TASK_DOWNLOAD_SERVICE;
            case TaskIds.DOWNLOAD_CLEANUP_JOB_ID:
                return BACKGROUND_TASK_DOWNLOAD_CLEANUP;
            case TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID:
                return BACKGROUND_TASK_WEBVIEW_VARIATIONS;
            default:
                assert false;
        }
        // Returning a value that is not expected to ever be reported.
        return BACKGROUND_TASK_TEST;
    }

    @VisibleForTesting
    static Set<String> getCachedUmaEntries(SharedPreferences prefs) {
        return prefs.getStringSet(KEY_CACHED_UMA, new HashSet<String>(1));
    }

    @VisibleForTesting
    static void updateCachedUma(SharedPreferences prefs, Set<String> cachedUma) {
        ThreadUtils.assertOnUiThread();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putStringSet(KEY_CACHED_UMA, cachedUma);
        editor.apply();
    }

    void assertNativeIsLoaded() {
        assert LibraryLoader.isInitialized();
    }
}
