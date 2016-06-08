// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

/**
 * Unit tests for BackgroundOfflinerTask.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        application = BaseChromiumApplication.class)
public class BackgroundOfflinerTaskTest {
    private Bundle mTaskExtras;
    private long mTestTime;
    private StubBackgroundSchedulerProcessor mStubBackgroundSchedulerProcessor;

    @Before
    public void setUp() throws Exception {
        // Build a bundle
        mTaskExtras = new Bundle();
        TaskExtrasPacker.packTimeInBundle(mTaskExtras);
        mStubBackgroundSchedulerProcessor = new StubBackgroundSchedulerProcessor();
    }

    @Test
    @Feature({"OfflinePages"})
    public void testIncomingTask() {
        BackgroundOfflinerTask task =
                new BackgroundOfflinerTask(mStubBackgroundSchedulerProcessor);
        task.processBackgroundRequests(mTaskExtras);

        // Check with ShadowBackgroundBackgroundSchedulerProcessor that startProcessing got called.
        assertTrue(mStubBackgroundSchedulerProcessor.getStartProcessingCalled());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testCallback() {
        // TODO(petewil): Implement the test
    }
}
