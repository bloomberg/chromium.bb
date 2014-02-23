// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.pm.ActivityInfo;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.Surface;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for Screen Orientation API (and legacy API).
 */
public class ScreenOrientationTest extends ContentShellTestBase {

    private static final String DEFAULT_URL =
            UrlUtils.encodeHtmlDataUri("<html><body>foo</body></html>");

    /**
     * Whether we should allow 0 degrees when expecting 180 degrees.
     * Try server bots, for some yet unknown reasons, rotate to 0 degrees angle
     * when they should rotate to 180 degrees. See http://crbug.com/344461
     */
    private static final boolean ALLOW_0_FOR_180 = true;

    /**
     * Criteria used to know when an orientation change happens.
     */
    private class OrientationChangeListenerCriteria implements Criteria {

        private final int mTargetOrientation;

        private OrientationChangeListenerCriteria(int targetOrientation) {
            mTargetOrientation = targetOrientation;
        }

        @Override
        public boolean isSatisfied() {
            return getOrientationAngle() == mTargetOrientation;
        }
    }

    private ContentView mContentView;

    /**
     * Returns the current device orientation angle.
     */
    private int getOrientationAngle() {
        switch (getActivity().getWindowManager().getDefaultDisplay()
                .getRotation()) {
            case Surface.ROTATION_90:
                return 90;
            case Surface.ROTATION_180:
                return 180;
            case Surface.ROTATION_270:
                return 270;
            case Surface.ROTATION_0:
                return 0;
            default:
                fail("Should not be there!");
                return 0;
        }
    }

    /**
     * Returns the expected orientation angle for a specific orientation type as
     * defined in ActivityInfor.screenOrientation.
     */
    private static int orientationTypeToAngle(int orientation) {
        switch (orientation) {
            case ActivityInfo.SCREEN_ORIENTATION_PORTRAIT:
                return 0;
            case ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE:
                return 90;
            case ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT:
                return 180;
            case ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE:
                return 270;
            default:
                fail("Should not be there!");
                return 0;
        }
    }

    /**
     * Locks the screen orientation to the predefined orientation type.
     */
    private void lockOrientation(int orientation) {
        getActivity().setRequestedOrientation(orientation);
    }

    /**
     * Unlock the screen orientation. Equivalent to locking to unspecified.
     */
    private void unlockOrientation() {
        getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    /**
     * Locks the screen orientation to the predefined orientation type then wait
     * for the orientation change to happen.
     */
    private boolean lockOrientationAndWait(int orientation)
            throws InterruptedException {
        OrientationChangeListenerCriteria listenerCriteria =
                new OrientationChangeListenerCriteria(orientationTypeToAngle(orientation));

        lockOrientation(orientation);

        return CriteriaHelper.pollForCriteria(listenerCriteria);
    }

    /**
     * Returns the screen orientation as seen by |window.orientation|.
     */
    private int getWindowOrientation()
            throws InterruptedException, TimeoutException {
        return Integer.parseInt(
            JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    mContentView,
                    new TestCallbackHelperContainer(mContentView),
                    "window.orientation"));
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        ContentShellActivity activity = launchContentShellWithUrl(DEFAULT_URL);
        waitForActiveShellToBeDoneLoading();
        mContentView = activity.getActiveContentView();
    }

    @Override
    public void tearDown() throws Exception {
        unlockOrientation();
        super.tearDown();
    }

    @SmallTest
    @Feature({"ScreenOrientation", "Main"})
    public void testDefault() throws Throwable {
        assertEquals(0, getWindowOrientation());
    }

    @MediumTest
    @Feature({"ScreenOrientation", "Main"})
    public void testChanges() throws Throwable {
        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertEquals(0, getWindowOrientation());

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertEquals(90, getWindowOrientation());

        lockOrientationAndWait(
                ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        int orientation = getWindowOrientation();
        assertTrue(180 == orientation || (0 == orientation && ALLOW_0_FOR_180));

        lockOrientationAndWait(
                ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        assertEquals(-90, getWindowOrientation());

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertEquals(0, getWindowOrientation());
    }

    @MediumTest
    @Feature({"ScreenOrientation", "Main"})
    public void testFlipChangePortrait() throws Throwable {
        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertEquals(0, getWindowOrientation());

        lockOrientationAndWait(
            ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        int orientation = getWindowOrientation();
        assertTrue(180 == orientation || (0 == orientation && ALLOW_0_FOR_180));
    }

    @MediumTest
    @Feature({"ScreenOrientation", "Main"})
    public void testFlipChangeLandscape() throws Throwable {
        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertEquals(90, getWindowOrientation());

        lockOrientationAndWait(
            ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        assertEquals(-90, getWindowOrientation());
    }
}
