// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.ContentJUnit4ClassRunner;
import org.chromium.device.geolocation.LocationProviderAdapter;

/**
 * Test suite for LocationProvider.
 */
@RunWith(ContentJUnit4ClassRunner.class)
public class LocationProviderTest {
    private Activity mActivity;
    private LocationProviderAdapter mLocationProvider;

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    @Before
    @SuppressFBWarnings("URF_UNREAD_FIELD")
    public void setUp() {
        mActivity = new Activity();
        mLocationProvider = LocationProviderAdapter.create(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
    }

    /**
     * Verify a normal start/stop call pair without any activity pauses.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testStartStop() throws Exception {
        mLocationProvider.start(false);
        Assert.assertTrue("Should be running", mLocationProvider.isRunning());
        mLocationProvider.stop();
        Assert.assertFalse("Should have stopped", mLocationProvider.isRunning());
    }

    /**
     * Verify a start/upgrade/stop call sequence without any activity pauses.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testStartUpgradeStop() throws Exception {
        mLocationProvider.start(false);
        Assert.assertTrue("Should be running", mLocationProvider.isRunning());
        mLocationProvider.start(true);
        Assert.assertTrue("Should be running", mLocationProvider.isRunning());
        mLocationProvider.stop();
        Assert.assertFalse("Should have stopped", mLocationProvider.isRunning());
    }
}
