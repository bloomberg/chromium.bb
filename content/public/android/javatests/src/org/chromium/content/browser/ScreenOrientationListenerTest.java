// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.MockOrientationObserver;
import org.chromium.content.browser.test.util.OrientationChangeObserverCriteria;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;
import org.chromium.ui.gfx.DeviceDisplayInfo;

/**
 * Tests for ScreenOrientationListener and its implementations.
 */
public class ScreenOrientationListenerTest extends ContentShellTestBase {

    // For some reasons build bots are not able to lock to 180 degrees. This
    // boolean is here to make the false negative go away in that situation.
    private static final boolean ALLOW_0_FOR_180 = true;

    private static final String DEFAULT_URL =
            UrlUtils.encodeHtmlDataUri("<html><body>foo</body></html>");

    private MockOrientationObserver mObserver;

    private int mNaturalOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

    /**
     * Checks does the device orientation match the requested one.
     */
    private boolean checkOrientationForLock(int orientation) {
        int expectedOrientation = orientationTypeToAngle(orientation);
        int currentOrientation = mObserver.mOrientation;
        switch (orientation) {
            case ActivityInfo.SCREEN_ORIENTATION_PORTRAIT:
            case ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE:
            case ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT:
            case ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE:
                if (expectedOrientation == currentOrientation) {
                    return true;
                } else if (ALLOW_0_FOR_180 && expectedOrientation == 180
                        && currentOrientation == 0) {
                    return true;
                }
                return false;
            default:
                return false;
        }
    }

    /**
     * Returns the expected orientation angle based on the orientation type.
     */
    private int orientationTypeToAngle(int orientation) {
        if (mNaturalOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT) {
            switch (orientation) {
                case ActivityInfo.SCREEN_ORIENTATION_PORTRAIT:
                    return 0;
                case ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE:
                    return 90;
                case ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT:
                    return 180;
                case ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE:
                    return -90;
                default:
                    fail("Should not be there!");
                    return 0;
            }
        } else { // mNaturalOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            switch (orientation) {
                case ActivityInfo.SCREEN_ORIENTATION_PORTRAIT:
                    return -90;
                case ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE:
                    return 0;
                case ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT:
                    return 90;
                case ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE:
                    return 180;
                default:
                    fail("Should not be there!");
                    return 0;
            }
        }
    }

   /**
    * Retrieves device natural orientation.
    */
    private int getNaturalOrientation(Activity activity) {
        DeviceDisplayInfo displayInfo = DeviceDisplayInfo.create(activity);
        int rotation = displayInfo.getRotationDegrees();
        if (rotation == 0 || rotation == 180) {
            if (displayInfo.getDisplayHeight() >= displayInfo.getDisplayWidth()) {
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            }
            return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        } else {
            if (displayInfo.getDisplayHeight() < displayInfo.getDisplayWidth()) {
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            }
            return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        }
    }

    /**
     * Locks the screen orientation to the predefined orientation type then wait
     * for the orientation change to happen.
     */
    private void lockOrientationAndWait(final int orientation) throws InterruptedException {
        OrientationChangeObserverCriteria criteria =
                new OrientationChangeObserverCriteria(mObserver,
                                                      orientationTypeToAngle(orientation));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().setRequestedOrientation(orientation);
            }
        });
        getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollForCriteria(criteria);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mObserver = new MockOrientationObserver();

        final ContentShellActivity activity = launchContentShellWithUrl(DEFAULT_URL);
        waitForActiveShellToBeDoneLoading();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ScreenOrientationListener.getInstance().addObserver(mObserver, activity);
                ScreenOrientationListener.getInstance().startAccurateListening();
            }
        });

        // Calculate device natural orientation, as mObserver.mOrientation
        // is difference between current and natural orientation in degrees.
        mNaturalOrientation = getNaturalOrientation(activity);

        // Make sure we start all the tests with the same orientation.
        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    }

    @Override
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
                ScreenOrientationListener.getInstance().startAccurateListening();
            }
        });

        mObserver = null;
        super.tearDown();
    }

    @MediumTest
    @Feature({"ScreenOrientation"})
    public void testVariousOrientationChanges() throws Exception {
        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertTrue(checkOrientationForLock(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE));

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        assertTrue(checkOrientationForLock(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT));

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        assertTrue(checkOrientationForLock(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE));

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertTrue(checkOrientationForLock(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT));
    }

    @MediumTest
    @Feature({"ScreenOrientation"})
    public void testFlipPortrait() throws Exception {
        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertTrue(checkOrientationForLock(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT));

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        assertTrue(checkOrientationForLock(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT));
    }

    @MediumTest
    @Feature({"ScreenOrientation"})
    public void testFlipLandscape() throws Exception {
        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertTrue(checkOrientationForLock(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE));

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        assertTrue(checkOrientationForLock(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE));
    }
}
