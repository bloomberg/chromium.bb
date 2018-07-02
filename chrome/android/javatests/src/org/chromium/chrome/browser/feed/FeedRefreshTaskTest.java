// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link FeedSchedulerBridge}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class FeedRefreshTaskTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static class TestBackgroundTaskScheduler implements BackgroundTaskScheduler {
        private List<TaskInfo> mTaskInfoList = new ArrayList<>();
        private List<Integer> mCanceledTaskIds = new ArrayList<>();

        public List<TaskInfo> getTaskInfoList() {
            return mTaskInfoList;
        }

        public List<Integer> getCanceledTaskIds() {
            return mCanceledTaskIds;
        }

        @Override
        public boolean schedule(Context context, TaskInfo taskInfo) {
            mTaskInfoList.add(taskInfo);
            return true;
        }

        @Override
        public void cancel(Context context, int taskId) {
            mCanceledTaskIds.add(taskId);
        }

        @Override
        public void checkForOSUpgrade(Context context) {}

        @Override
        public void reschedule(Context context) {}
    }

    private TestBackgroundTaskScheduler mTaskScheduler;

    @Before
    public void setUp() throws InterruptedException {
        mTaskScheduler = new TestBackgroundTaskScheduler();
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    public void testSchedule() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0, mTaskScheduler.getTaskInfoList().size());
            FeedRefreshTask.scheduleWakeUp(/*thresholdMs=*/1234);
            Assert.assertEquals(1, mTaskScheduler.getTaskInfoList().size());
            TaskInfo actualInfo = mTaskScheduler.getTaskInfoList().get(0);
            Assert.assertEquals(TaskIds.FEED_REFRESH_JOB_ID, actualInfo.getTaskId());
            Assert.assertEquals(TaskInfo.NETWORK_TYPE_ANY, actualInfo.getRequiredNetworkType());
            Assert.assertEquals(false, actualInfo.requiresCharging());
            Assert.assertEquals(true, actualInfo.isPeriodic());
            Assert.assertEquals(true, actualInfo.isPersisted());
            Assert.assertEquals(true, actualInfo.shouldUpdateCurrent());
            // 1234 * 1.1 = 1357
            Assert.assertEquals(1357, actualInfo.getPeriodicInfo().getIntervalMs());
            // 1234 * 0.2 = 246
            Assert.assertEquals(246, actualInfo.getPeriodicInfo().getFlexMs());
        });
    }

    @Test
    @SmallTest
    public void testCancelWakeUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0, mTaskScheduler.getCanceledTaskIds().size());
            FeedRefreshTask.cancelWakeUp();
            Assert.assertEquals(1, mTaskScheduler.getCanceledTaskIds().size());
        });
    }

    @Test
    @SmallTest
    public void testReschedule() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // The FeedSchedulerHost might create a task during its initialization, so clear
            // out anything before reschedule() is called.
            FeedProcessScopeFactory.getFeedSchedulerBridge();
            mTaskScheduler.getTaskInfoList().clear();
            Assert.assertEquals(0, mTaskScheduler.getTaskInfoList().size());

            new FeedRefreshTask().reschedule(mActivityTestRule.getActivity());
            Assert.assertEquals(1, mTaskScheduler.getTaskInfoList().size());
        });
    }
}
