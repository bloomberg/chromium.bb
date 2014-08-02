// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.pm.ActivityInfo;
import android.os.Build;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
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
    private void lockOrientationAndWait(final int orientation) throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    lockOrientation(orientation);
                } catch (Exception e) {
                    fail("Should not be there!");
                }
            }
        });
        // TODO(mlamouri): this slows the tests down, we could consider removing
        // this if it doesn't flakes the test.
        getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollForCriteria(
            new OrientationChangeObserverCriteria(mObserver, orientationTypeToAngle(orientation)));
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

        final ContentShellActivity activity = launchContentShellWithUrl(DEFAULT_URL);
        waitForActiveShellToBeDoneLoading();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ScreenOrientationListener.getInstance().addObserver(mObserver, activity);
            }
        });

        // Make sure we start all the tests with the same orientation.
        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    }

    @Override
    public void tearDown() throws Exception {
        unlockOrientation();

        mObserver = null;
        super.tearDown();
    }

    @LargeTest
    @Feature({"ScreenOrientation"})
    public void testVariousOrientationChanges() throws Exception {
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
    public void testFlipPortrait() throws Exception {
        // This can't work for pre JB-MR1 SDKs.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1)
            return;

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertEquals(0, mObserver.mOrientation);

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        assertTrue(mObserver.mOrientation == 180 ||
                   (ALLOW_0_FOR_180 && mObserver.mOrientation == 0));
    }

    @MediumTest
    @Feature({"ScreenOrientation"})
    public void testFlipLandscape() throws Exception {
        // This can't work for pre JB-MR1 SDKs.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1)
            return;

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertEquals(90, mObserver.mOrientation);

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        assertEquals(-90, mObserver.mOrientation);
    }
}
