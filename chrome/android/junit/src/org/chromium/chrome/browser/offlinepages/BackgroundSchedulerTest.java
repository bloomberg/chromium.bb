// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import com.google.android.gms.gcm.Task;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

/**
 * Unit tests for BackgroundScheduler.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        application = BaseChromiumApplication.class,
        shadows = {ShadowGcmNetworkManager.class})
public class BackgroundSchedulerTest {
    private Context mContext;
    private TriggerConditions mConditions1 = new TriggerConditions(
            true /* power */, 10 /* battery percentage */, false /* unmetered */);

    @Before
    public void setUp() throws Exception {
        mContext =  Robolectric.application;
        ShadowGcmNetworkManager.clear();
    }

    @Test
    @Feature({"OfflinePages"})
    public void testSchedule() {
        BackgroundScheduler scheduler = new BackgroundScheduler();
        assertNull(ShadowGcmNetworkManager.getScheduledTask());
        scheduler.schedule(mContext, mConditions1);
        // Check with gcmNetworkManagerShadow that schedule got called.
        assertNotNull(ShadowGcmNetworkManager.getScheduledTask());

        // Verify details of the scheduled task.
        Task task = ShadowGcmNetworkManager.getScheduledTask();
        assertEquals(OfflinePageUtils.TASK_TAG, task.getTag());
        long scheduledTimeMillis = TaskExtrasPacker.unpackTimeFromBundle(task.getExtras());
        assertTrue(scheduledTimeMillis > 0L);
        assertEquals(
                mConditions1, TaskExtrasPacker.unpackTriggerConditionsFromBundle(task.getExtras()));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testUnschedule() {
        BackgroundScheduler scheduler = new BackgroundScheduler();
        assertNull(ShadowGcmNetworkManager.getScheduledTask());
        scheduler.schedule(mContext, mConditions1);
        assertNotNull(ShadowGcmNetworkManager.getScheduledTask());

        assertNull(ShadowGcmNetworkManager.getCanceledTask());
        scheduler.unschedule(mContext);
        assertNotNull(ShadowGcmNetworkManager.getCanceledTask());
    }
}
