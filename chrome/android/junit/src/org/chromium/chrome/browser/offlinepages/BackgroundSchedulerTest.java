// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.Context;

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

    @Before
    public void setUp() throws Exception {
        mContext =  Robolectric.application;
    }

    @Test
    @Feature({"OfflinePages"})
    public void testSchedule() {
        BackgroundScheduler scheduler = new BackgroundScheduler();
        assertNull(ShadowGcmNetworkManager.getScheduledTask());
        scheduler.schedule(mContext);
        // Check with gcmNetworkManagerShadow that schedule got called.
        assertNotNull(ShadowGcmNetworkManager.getScheduledTask());

        // TODO(petewil): Also assert that the date we see is what we expected
    }

    @Test
    @Feature({"OfflinePages"})
    public void testUnschedule() {
        // TODO(petewil): Add this test.
    }
}
