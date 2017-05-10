// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.chrome.browser.multiwindow.MultiWindowUtilsTest.createSecondChromeTabbedActivity;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Integration testing for Android's N+ MultiWindow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
@MinAndroidSdkLevel(Build.VERSION_CODES.N)
public class MultiWindowIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature("MultiWindow")
    @TargetApi(Build.VERSION_CODES.N)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    public void testIncognitoNtpHandledCorrectly() throws InterruptedException {
        try {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    FirstRunStatus.setFirstRunFlowComplete(true);
                }
            });

            mActivityTestRule.newIncognitoTabFromMenu();
            Assert.assertTrue(mActivityTestRule.getActivity().getActivityTab().isIncognito());
            final int incognitoTabId = mActivityTestRule.getActivity().getActivityTab().getId();
            final ChromeTabbedActivity2 cta2 =
                    createSecondChromeTabbedActivity(mActivityTestRule.getActivity());

            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    Context context = ContextUtils.getApplicationContext();
                    ActivityManager activityManager =
                            (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
                    for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
                        if (mActivityTestRule.getActivity().getTaskId() == task.getTaskInfo().id) {
                            task.moveToFront();
                            break;
                        }
                    }
                }
            });
            CriteriaHelper.pollUiThread(Criteria.equals(ActivityState.RESUMED,
                    new Callable<Integer>() {
                        @Override
                        public Integer call() throws Exception {
                            return ApplicationStatus.getStateForActivity(
                                    mActivityTestRule.getActivity());
                        }
                    }));

            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), R.id.move_to_other_window_menu_id);

            CriteriaHelper.pollUiThread(Criteria.equals(1,
                    new Callable<Integer>() {
                        @Override
                        public Integer call() throws Exception {
                            return cta2.getTabModelSelector().getModel(true).getCount();
                        }
                    }));

            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    Assert.assertEquals(1, TabWindowManager.getInstance().getIncognitoTabCount());

                    // Ensure the same tab exists in the new activity.
                    Assert.assertEquals(incognitoTabId, cta2.getActivityTab().getId());
                }
            });
        } finally {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    FirstRunStatus.setFirstRunFlowComplete(false);
                }
            });
        }
    }

}
