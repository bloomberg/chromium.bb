// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Looper;
import android.test.UiThreadTest;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ActivityStatus;
import org.chromium.base.test.Feature;

/**
 * Test suite for LocationProvider.
 */
public class LocationProviderTest extends InstrumentationTestCase {
    private LocationProvider mLocationProvider;

    @Override
    public void setUp() {
        mLocationProvider = LocationProvider.create(getInstrumentation().getTargetContext());
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

    /**
     * Verify that pausing the activity stops location listener and when
     * activity resumes it restarts listening.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testStartPauseResumeStop() throws Exception {
        mLocationProvider.start(false);
        assertTrue("Should be running", mLocationProvider.isRunning());
        ActivityStatus.getInstance().onPause();
        assertFalse("Should have paused", mLocationProvider.isRunning());
        ActivityStatus.getInstance().onResume();
        assertTrue("Should have resumed", mLocationProvider.isRunning());
        mLocationProvider.stop();
        assertFalse("Should have stopped", mLocationProvider.isRunning());
    }

    /**
     * Verify that calling start when the activity is paused doesn't start listening
     * for location updates until activity resumes.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testPauseStartResumeStop() throws Exception {
        ActivityStatus.getInstance().onPause();
        mLocationProvider.start(false);
        assertFalse("Should not be running", mLocationProvider.isRunning());
        ActivityStatus.getInstance().onResume();
        assertTrue("Should have resumed", mLocationProvider.isRunning());
        mLocationProvider.stop();
        assertFalse("Should have stopped", mLocationProvider.isRunning());
    }

    /**
     * Verify that upgrading when paused works as expected.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testStartPauseUpgradeResumeStop() throws Exception {
        mLocationProvider.start(false);
        assertTrue("Should be running", mLocationProvider.isRunning());
        ActivityStatus.getInstance().onPause();
        assertFalse("Should have paused", mLocationProvider.isRunning());
        mLocationProvider.start(true);
        assertFalse("Should be paused", mLocationProvider.isRunning());
        ActivityStatus.getInstance().onResume();
        assertTrue("Should have resumed", mLocationProvider.isRunning());
        mLocationProvider.stop();
        assertFalse("Should have stopped", mLocationProvider.isRunning());
    }
}
