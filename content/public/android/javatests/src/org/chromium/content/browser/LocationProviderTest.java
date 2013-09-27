// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.test.UiThreadTest;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ActivityStatus;
import org.chromium.base.test.util.Feature;

/**
 * Test suite for LocationProvider.
 */
public class LocationProviderTest extends InstrumentationTestCase {
    private Activity mActivity;
    private LocationProvider mLocationProvider;

    @Override
    public void setUp() {
        mActivity = new Activity();
        mLocationProvider = LocationProvider.create(getInstrumentation().getTargetContext());
    }

    /**
     * Verify a normal start/stop call pair without any activity pauses.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testStartStop() throws Exception {
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.RESUMED);
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
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.RESUMED);
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
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.RESUMED);
        mLocationProvider.start(false);
        assertTrue("Should be running", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.PAUSED);
        assertFalse("Should have paused", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.RESUMED);
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
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.PAUSED);
        mLocationProvider.start(false);
        assertFalse("Should not be running", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.RESUMED);
        assertTrue("Should have resumed", mLocationProvider.isRunning());
        mLocationProvider.stop();
        assertFalse("Should have stopped", mLocationProvider.isRunning());
    }

    /**
     * Verify that calling start when the activity is being created doesn't start listening
     * for location updates until activity resumes.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testCreatedStartedStartResumeStop() throws Exception {
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.CREATED);
        mLocationProvider.start(false);
        assertFalse("Should not be running", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.STARTED);
        mLocationProvider.start(false);
        assertFalse("Should not be running", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.RESUMED);
        assertTrue("Should have resumed", mLocationProvider.isRunning());
        mLocationProvider.stop();
        assertFalse("Should have stopped", mLocationProvider.isRunning());
    }

    /**
     * Verify that calling start when the activity is being created then immediately paused doesn't
     * start listening for location updates until activity resumes.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testCreatedStartedStartPausedResumeStop() throws Exception {
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.CREATED);
        mLocationProvider.start(false);
        assertFalse("Should not be running", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.STARTED);
        mLocationProvider.start(false);
        assertFalse("Should not be running", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.PAUSED);
        assertFalse("Should not be running", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.RESUMED);
        assertTrue("Should have resumed", mLocationProvider.isRunning());
        mLocationProvider.stop();
        assertFalse("Should have stopped", mLocationProvider.isRunning());
    }

    /**
     * Verify that calling start when the activity is stopped doesn't start listening
     * for location updates until activity resumes.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Location"})
    public void testStopStartResumeStop() throws Exception {
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.STOPPED);
        mLocationProvider.start(false);
        assertFalse("Should not be running", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.RESUMED);
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
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.RESUMED);
        mLocationProvider.start(false);
        assertTrue("Should be running", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.PAUSED);
        assertFalse("Should have paused", mLocationProvider.isRunning());
        mLocationProvider.start(true);
        assertFalse("Should be paused", mLocationProvider.isRunning());
        ActivityStatus.onStateChangeForTesting(mActivity, ActivityStatus.RESUMED);
        assertTrue("Should have resumed", mLocationProvider.isRunning());
        mLocationProvider.stop();
        assertFalse("Should have stopped", mLocationProvider.isRunning());
    }
}
