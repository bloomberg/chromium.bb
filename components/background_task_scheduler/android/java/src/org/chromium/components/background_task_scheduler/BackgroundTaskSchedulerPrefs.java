// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;

import java.util.HashSet;
import java.util.Set;

/**
 * Class handling shared preference entries for BackgroundTaskScheduler.
 */
public class BackgroundTaskSchedulerPrefs {
    private static final String TAG = "BTSPrefs";
    private static final String KEY_SCHEDULED_TASKS = "bts_scheduled_tasks";
    private static final String KEY_LAST_OS_VERSION = "bts_last_os_version";
    private static final String KEY_LAST_APP_VERSION = "bts_last_app_version";
    private static final String ENTRY_SEPARATOR = ":";

    /** Adds a task to scheduler's preferences, so that it can be rescheduled with OS upgrade. */
    public static void addScheduledTask(TaskInfo taskInfo) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> scheduledTasks =
                prefs.getStringSet(KEY_SCHEDULED_TASKS, new HashSet<String>(1));
        String prefsEntry = toSharedPreferenceEntry(taskInfo);
        if (scheduledTasks.contains(prefsEntry)) return;

        // Set returned from getStringSet() should not be modified.
        scheduledTasks = new HashSet<>(scheduledTasks);
        scheduledTasks.add(prefsEntry);
        updateScheduledTasks(prefs, scheduledTasks);
    }

    /** Removes a task from scheduler's preferences. */
    public static void removeScheduledTask(int taskId) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> scheduledTasks = prefs.getStringSet(KEY_SCHEDULED_TASKS, new HashSet<String>());

        String entryToRemove = null;
        String taskSuffix = ENTRY_SEPARATOR + taskId;
        for (String entry : scheduledTasks) {
            if (entry.endsWith(taskSuffix)) {
                entryToRemove = entry;
                break;
            }
        }

        // Entry matching taskId was not found.
        if (entryToRemove == null) return;

        // Set returned from getStringSet() should not be modified.
        scheduledTasks = new HashSet<>(scheduledTasks);
        scheduledTasks.remove(entryToRemove);
        updateScheduledTasks(prefs, scheduledTasks);
    }

    /** Gets a set of scheduled task class names. */
    public static Set<String> getScheduledTasks() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> prefsEntries = prefs.getStringSet(KEY_SCHEDULED_TASKS, new HashSet<String>(1));
        Set<String> scheduledTasksClassNames = new HashSet<>(prefsEntries.size());
        for (String entry : prefsEntries) {
            String[] entryParts = entry.split(ENTRY_SEPARATOR);
            if (entryParts.length != 2 || entryParts[0] == null || entryParts[0].isEmpty()) {
                continue;
            }
            scheduledTasksClassNames.add(entryParts[0]);
        }

        return scheduledTasksClassNames;
    }

    /** Removes all scheduled tasks from shared preferences store. */
    public static void removeAllTasks() {
        ContextUtils.getAppSharedPreferences().edit().remove(KEY_SCHEDULED_TASKS).apply();
    }

    private static void updateScheduledTasks(SharedPreferences prefs, Set<String> tasks) {
        SharedPreferences.Editor editor = prefs.edit();
        editor.putStringSet(KEY_SCHEDULED_TASKS, tasks);
        editor.apply();
    }

    /**
     * Converts a task info to a shared preference entry in the format:
     * BACKGROUND_TASK_CLASS_NAME:TASK_ID.
     * Task ID is necessary to be able to remove the entries from the shared preferences.
     */
    private static String toSharedPreferenceEntry(TaskInfo taskInfo) {
        return taskInfo.getBackgroundTaskClass().getName() + ENTRY_SEPARATOR + taskInfo.getTaskId();
    }
}
