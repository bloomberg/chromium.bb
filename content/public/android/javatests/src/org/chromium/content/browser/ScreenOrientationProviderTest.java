// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Build;
import android.test.FlakyTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.MockOrientationObserver;
import org.chromium.content.browser.test.util.OrientationChangeObserverCriteria;
import org.chromium.content.common.ScreenOrientationValues;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Tests for ScreenOrientationListener and its implementations.
 */
public class ScreenOrientationProviderTest extends ContentShellTestBase {

    // For some reasons build bots are not able to lock to 180 degrees. This
    // boolean is here to make the false negative go away in that situation.
    private static final boolean ALLOW_0_FOR_180 = true;

    private static final String DEFAULT_URL =
            UrlUtils.encodeHtmlDataUri("<html><body>foo</body></html>");

    private MockOrientationObserver mObserver;

    private boolean checkOrientationForLock(int orientations) {
        switch (orientations) {
            case ScreenOrientationValues.PORTRAIT_PRIMARY:
                return mObserver.mOrientation == 0;
            case ScreenOrientationValues.PORTRAIT_SECONDARY:
                return mObserver.mOrientation == 180 ||
                       (ALLOW_0_FOR_180 && mObserver.mOrientation == 0);
            case ScreenOrientationValues.LANDSCAPE_PRIMARY:
                return mObserver.mOrientation == 90;
            case ScreenOrientationValues.LANDSCAPE_SECONDARY:
                return mObserver.mOrientation == -90;
            case ScreenOrientationValues.PORTRAIT:
                return mObserver.mOrientation == 0 || mObserver.mOrientation == 180;
            case ScreenOrientationValues.LANDSCAPE:
                return mObserver.mOrientation == 90 || mObserver.mOrientation == -90;
            case ScreenOrientationValues.ANY:
                // The orientation should not change but might and the value could be anything.
                return true;
            default:
                return mObserver.mHasChanged == false;
        }
    }

    /**
     * Locks the screen orientation to |orientations| using ScreenOrientationProvider.
     */
    private void lockOrientation(int orientations) {
        ScreenOrientationProvider.lockOrientation((byte)orientations);
    }

    /**
     * Call |lockOrientation| and wait for an orientation change.
     */
    private boolean lockOrientationAndWait(int orientations)
            throws InterruptedException {
        OrientationChangeObserverCriteria criteria =
                new OrientationChangeObserverCriteria(mObserver);

        lockOrientation(orientations);

        return CriteriaHelper.pollForCriteria(criteria);
    }

    /**
     * Unlock the screen orientation using |ScreenOrientationProvider|.
     */
    private void unlockOrientation() {
        ScreenOrientationProvider.unlockOrientation();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        ContentShellActivity activity = launchContentShellWithUrl(DEFAULT_URL);
        waitForActiveShellToBeDoneLoading();

        mObserver = new MockOrientationObserver();
        ScreenOrientationListener.getInstance().addObserver(
                mObserver, getInstrumentation().getTargetContext());

        // Make sure mObserver is updated before we start the tests.
        OrientationChangeObserverCriteria criteria =
                new OrientationChangeObserverCriteria(mObserver);
        CriteriaHelper.pollForCriteria(criteria);
    }

    @Override
    public void tearDown() throws Exception {
        unlockOrientation();

        mObserver = null;
        super.tearDown();
    }

    // @SmallTest
    // crbug.com/353500
    @FlakyTest
    @Feature({"ScreenOrientation"})
    public void testBasicValues() throws Exception {
        lockOrientationAndWait(ScreenOrientationValues.PORTRAIT_PRIMARY);
        assertTrue(checkOrientationForLock(ScreenOrientationValues.PORTRAIT_PRIMARY));

        lockOrientationAndWait(ScreenOrientationValues.LANDSCAPE_PRIMARY);
        assertTrue(checkOrientationForLock(ScreenOrientationValues.LANDSCAPE_PRIMARY));

        lockOrientationAndWait(ScreenOrientationValues.PORTRAIT_SECONDARY);
        assertTrue(checkOrientationForLock(ScreenOrientationValues.PORTRAIT_SECONDARY));

        lockOrientationAndWait(ScreenOrientationValues.LANDSCAPE_SECONDARY);
        assertTrue(checkOrientationForLock(ScreenOrientationValues.LANDSCAPE_SECONDARY));
    }

    // @MediumTest
    // crbug.com/353500
    @FlakyTest
    @Feature({"ScreenOrientation"})
    public void testPortrait() throws Exception {
        lockOrientationAndWait(ScreenOrientationValues.PORTRAIT_PRIMARY);
        assertTrue(checkOrientationForLock(ScreenOrientationValues.PORTRAIT_PRIMARY));

        lockOrientationAndWait(ScreenOrientationValues.PORTRAIT_PRIMARY |
                ScreenOrientationValues.PORTRAIT_SECONDARY);
        assertTrue(checkOrientationForLock(ScreenOrientationValues.PORTRAIT_PRIMARY |
                ScreenOrientationValues.PORTRAIT_SECONDARY));

        lockOrientationAndWait(ScreenOrientationValues.PORTRAIT_SECONDARY);
        assertTrue(checkOrientationForLock(ScreenOrientationValues.PORTRAIT_SECONDARY));

        lockOrientationAndWait(ScreenOrientationValues.PORTRAIT_PRIMARY |
                ScreenOrientationValues.PORTRAIT_SECONDARY);
        assertTrue(checkOrientationForLock(ScreenOrientationValues.PORTRAIT_PRIMARY |
                ScreenOrientationValues.PORTRAIT_SECONDARY));
    }

    // @MediumTest
    // crbug.com/353500
    @FlakyTest
    @Feature({"ScreenOrientation"})
    public void testLandscape() throws Exception {
        int initialOrientation = mObserver.mOrientation;

        lockOrientationAndWait(ScreenOrientationValues.LANDSCAPE_PRIMARY);
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1) {
            // If we were in LANDSCAPE_SECONDARY (90 degrees), old SDK will not
            // be able to catch this change correctly. However, we still want to
            // wait to not break the rest of the test.
            boolean result = checkOrientationForLock(ScreenOrientationValues.LANDSCAPE_PRIMARY);
            if (initialOrientation != -90)
                assertTrue(result);
        } else {
            assertTrue(checkOrientationForLock(ScreenOrientationValues.LANDSCAPE_PRIMARY));
        }

        lockOrientationAndWait(ScreenOrientationValues.LANDSCAPE_PRIMARY |
                ScreenOrientationValues.LANDSCAPE_SECONDARY);
        assertTrue(checkOrientationForLock(ScreenOrientationValues.LANDSCAPE_PRIMARY |
                ScreenOrientationValues.LANDSCAPE_SECONDARY));

        lockOrientationAndWait(ScreenOrientationValues.LANDSCAPE_SECONDARY);
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1) {
            // Exactly the opposite situation as above.
            boolean result = checkOrientationForLock(ScreenOrientationValues.LANDSCAPE_SECONDARY);
            if (initialOrientation == -90)
                assertTrue(result);
        } else {
            assertTrue(checkOrientationForLock(ScreenOrientationValues.LANDSCAPE_SECONDARY));
        }

        lockOrientationAndWait(ScreenOrientationValues.LANDSCAPE_PRIMARY |
                ScreenOrientationValues.LANDSCAPE_SECONDARY);
        assertTrue(checkOrientationForLock(ScreenOrientationValues.LANDSCAPE_PRIMARY |
                ScreenOrientationValues.LANDSCAPE_SECONDARY));
    }

    // There is no point in testing the case where we try to lock to
    // PORTRAIT_PRIMARY | PORTRAIT_SECONDARY | LANDSCAPE_PRIMARY | LANDSCAPE_SECONDARY
    // because with the device being likely flat during the test, locking to that
    // will be a no-op.

    // We can't test unlock because the device is likely flat during the test
    // and in that situation unlocking is a no-op.
}
