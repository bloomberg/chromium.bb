// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

/**
 * Test suite for LocationProvider.
 */
public class LocationProviderTest extends InstrumentationTestCase {
    private Activity mActivity;
    private LocationProviderAdapter mLocationProvider;

    @Override
    public void setUp() {
        mActivity = new Activity();
        mLocationProvider =
                LocationProviderAdapter.create(getInstrumentation().getTargetContext());
    }

    /**
     * Verify a normal start/stop call pair without any activity pauses.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testStartStop() throws Exception {
        mLocationProvider.start(false);
        assertTrue("Should be running", mLocationProvider.isRunning());
        mLocationProvider.stop();
        assertFalse("Should have stopped", mLocationProvider.isRunning());
    }

    /**
     * Verify a start/upgrade/stop call sequence without any activity pauses.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testStartUpgradeStop() throws Exception {
        mLocationProvider.start(false);
        assertTrue("Should be running", mLocationProvider.isRunning());
        mLocationProvider.start(true);
        assertTrue("Should be running", mLocationProvider.isRunning());
        mLocationProvider.stop();
        assertFalse("Should have stopped", mLocationProvider.isRunning());
    }
}
