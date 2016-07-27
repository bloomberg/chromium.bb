// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.util.IntentUtils;

import java.lang.ref.WeakReference;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A simple activity that allows Chrome to expose print as an option in the share menu.
 */
public class PrintShareActivity extends AppCompatActivity {

    private static Set<Activity> sPendingShareActivities = new HashSet<>();
    private static ActivityStateListener sStateListener;

    /**
     * Enable the print sharing option.
     *
     * @param activity The activity that will be triggering the share action.  The activitiy's
     *                 state will be tracked to disable the print option when the share operation
     *                 has been completed.
     */
    public static void enablePrintShareOption(Activity activity) {
        ThreadUtils.assertOnUiThread();

        if (sStateListener == null) {
            sStateListener = new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity activity, int newState) {
                    if (newState == ActivityState.PAUSED) return;
                    unregisterActivity(activity);
                }
            };
        }
        ApplicationStatus.registerStateListenerForAllActivities(sStateListener);
        boolean wasEmpty = sPendingShareActivities.isEmpty();
        sPendingShareActivities.add(activity);
        if (wasEmpty) {
            activity.getPackageManager().setComponentEnabledSetting(
                    new ComponentName(activity, PrintShareActivity.class),
                    PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                    PackageManager.DONT_KILL_APP);
        }
    }

    private static void unregisterActivity(Activity activity) {
        sPendingShareActivities.remove(activity);
        if (!sPendingShareActivities.isEmpty()) return;
        ApplicationStatus.unregisterActivityStateListener(sStateListener);

        Context context = ContextUtils.getApplicationContext();
        context.getPackageManager().setComponentEnabledSetting(
                new ComponentName(context, PrintShareActivity.class),
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                PackageManager.DONT_KILL_APP);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            Intent intent = getIntent();
            if (intent == null) return;
            if (!Intent.ACTION_SEND.equals(intent.getAction())) return;
            if (!IntentUtils.safeHasExtra(getIntent(), ShareHelper.EXTRA_TASK_ID)) return;
            handlePrintAction();
        } finally {
            finish();
        }
    }

    private void handlePrintAction() {
        int triggeringTaskId =
                IntentUtils.safeGetIntExtra(getIntent(), ShareHelper.EXTRA_TASK_ID, 0);
        List<WeakReference<Activity>> activities = ApplicationStatus.getRunningActivities();
        ChromeActivity triggeringActivity = null;
        for (int i = 0; i < activities.size(); i++) {
            Activity activity = activities.get(i).get();
            if (activity == null) continue;

            // Since the share intent is triggered without NEW_TASK or NEW_DOCUMENT, the task ID
            // of this activity will match that of the triggering activity.
            if (activity.getTaskId() == triggeringTaskId
                    && activity instanceof ChromeActivity) {
                triggeringActivity = (ChromeActivity) activity;
                break;
            }
        }
        if (triggeringActivity == null) return;
        unregisterActivity(triggeringActivity);
        triggeringActivity.onMenuOrKeyboardAction(R.id.print_id, true);
    }

}
