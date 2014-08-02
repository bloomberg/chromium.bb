// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.pm.ActivityInfo;
import android.os.Build;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.MockOrientationObserver;
import org.chromium.content.browser.test.util.OrientationChangeObserverCriteria;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

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

    /**
     * Returns the expected orientation angle based on the orientation type.
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
     * Locks the screen orientation to the predefined orientation type then wait
     * for the orientation change to happen.
     */
    private boolean lockOrientationAndWait(int orientation)
            throws InterruptedException {
        OrientationChangeObserverCriteria criteria = new OrientationChangeObserverCriteria(
                mObserver, orientationTypeToAngle(orientation));

        lockOrientation(orientation);

        return CriteriaHelper.pollForCriteria(criteria);
    }

    /**
     * Unlock the screen orientation. Equivalent to locking to unspecified.
     */
    private void unlockOrientation() {
        getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mObserver = new MockOrientationObserver();
    }

    @Override
    public void tearDown() throws Exception {
        unlockOrientation();

        mObserver = null;
        super.tearDown();
    }

    private void setUpForConfigurationListener() throws InterruptedException {
        ScreenOrientationListener.getInstance().injectConfigurationListenerBackendForTest();

        ContentShellActivity activity = launchContentShellWithUrl(DEFAULT_URL);
        waitForActiveShellToBeDoneLoading();

        ScreenOrientationListener.getInstance().addObserver(
                mObserver, getInstrumentation().getTargetContext());
    }

    private boolean setUpForDisplayListener() throws InterruptedException {
        // This can't work for pre JB-MR1 SDKs.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1)
            return false;

        ContentShellActivity activity = launchContentShellWithUrl(DEFAULT_URL);
        waitForActiveShellToBeDoneLoading();

        ScreenOrientationListener.getInstance().addObserver(
                mObserver, getInstrumentation().getTargetContext());
        return true;
    }

    // At least one of these tests flakes 50% on all runs of
    // contentshell_instrumentation_tests.
    // crbug.com/356483
    /*
    @SmallTest
    @Feature({"ScreenOrientation"})
    public void testConfigurationListenerDefault() throws Exception {
        setUpForConfigurationListener();

        assertFalse(mObserver.mHasChanged);
        assertEquals(-1, mObserver.mOrientation);
    }

    @SmallTest
    @Feature({"ScreenOrientation"})
    public void testConfigurationListenerAsyncSetup() throws Exception {
        setUpForConfigurationListener();

        // We should get a onScreenOrientationChange call asynchronously.
        CriteriaHelper.pollForCriteria(new OrientationChangeObserverCriteria(
                mObserver));

        assertTrue(mObserver.mHasChanged);
        assertTrue(mObserver.mOrientation != -1);
    }

    @MediumTest
    @Feature({"ScreenOrientation"})
    public void testConfigurationListenerChanges() throws Exception {
        setUpForConfigurationListener();

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertEquals(90, mObserver.mOrientation);

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        assertTrue(mObserver.mOrientation == 180 ||
                   (ALLOW_0_FOR_180 && mObserver.mOrientation == 0));

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        assertEquals(-90, mObserver.mOrientation);

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertEquals(0, mObserver.mOrientation);
    }

    @MediumTest
    @Feature({"ScreenOrientation"})
    public void testConfigurationListenerFlipPortrait() throws Exception {
        setUpForConfigurationListener();

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertEquals(0, mObserver.mOrientation);

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        assertEquals(0, mObserver.mOrientation);
    }

    @MediumTest
    @Feature({"ScreenOrientation"})
    public void testConfigurationListenerFlipLandscape() throws Exception {
        setUpForConfigurationListener();

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertEquals(90, mObserver.mOrientation);

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        assertEquals(90, mObserver.mOrientation);
    }

    @SmallTest
    @Feature({"ScreenOrientation"})
    public void testDisplayListenerDefault() throws Exception {
        if (!setUpForDisplayListener())
            return;

        assertEquals(-1, mObserver.mOrientation);
    }

    @SmallTest
    @Feature({"ScreenOrientation"})
    public void testDisplayListenerAsyncSetup() throws Exception {
        if (!setUpForDisplayListener())
            return;

        // We should get a onScreenOrientationChange call asynchronously.
        CriteriaHelper.pollForCriteria(new OrientationChangeObserverCriteria(
                mObserver));

        assertTrue(mObserver.mOrientation != -1);
    }

    @MediumTest
    @Feature({"ScreenOrientation"})
    public void testDisplayListenerChanges() throws Exception {
        if (!setUpForDisplayListener())
            return;

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertEquals(90, mObserver.mOrientation);

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        assertTrue(mObserver.mOrientation == 180 ||
                   (ALLOW_0_FOR_180 && mObserver.mOrientation == 0));

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        assertEquals(-90, mObserver.mOrientation);

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertEquals(0, mObserver.mOrientation);
    }

    @SmallTest
    @Feature({"ScreenOrientation"})
    public void testDisplayListenerFlipPortrait() throws Exception {
        if (!setUpForDisplayListener())
            return;

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertEquals(0, mObserver.mOrientation);

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        assertTrue(mObserver.mOrientation == 180 ||
                   (ALLOW_0_FOR_180 && mObserver.mOrientation == 0));
    }

    @SmallTest
    @Feature({"ScreenOrientation"})
    public void testDisplayListenerFlipLandscape() throws Exception {
        if (!setUpForDisplayListener())
            return;

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertEquals(90, mObserver.mOrientation);

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        assertEquals(-90, mObserver.mOrientation);
    }
    */
}
