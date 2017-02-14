// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.share.OptionalShareTargetsManager;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;

import java.lang.ref.WeakReference;
import java.util.List;

/**
 * A simple activity that allows Chrome to expose print as an option in the share menu.
 */
public class PrintShareActivity extends AppCompatActivity {

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
        OptionalShareTargetsManager.handleShareFinish(triggeringActivity);
        triggeringActivity.onMenuOrKeyboardAction(R.id.print_id, true);
    }

    /**
     * Determines if the printing share option should be enabled.
     * @param currentTab The current tab of the triggering Chrome activity.
     * @return {@code true} if printing should be enabled.
     */
    public static boolean printingIsEnabled(Tab currentTab) {
        PrintingController printingController = PrintingControllerImpl.getInstance();
        return (printingController != null && !currentTab.isNativePage()
                && !printingController.isBusy()
                && PrefServiceBridge.getInstance().isPrintingEnabled());
    }
}
