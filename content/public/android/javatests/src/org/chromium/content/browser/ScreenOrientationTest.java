// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentViewCore.InternalAccessDelegate;
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
     * Class that is used to get notified when the activity configuration
     * changes.
     */
    private static class ConfigurationChangeAccessDelegate
            implements InternalAccessDelegate {

        private interface ConfigurationChangedListener {
            public void onConfigurationChanged();
        }

        private ConfigurationChangedListener mListener = null;

        private void setListener(ConfigurationChangedListener listener) {
            mListener = listener;
        }

        @Override
        public boolean drawChild(Canvas canvas, View child, long drawingTime) {
            return false;
        }

        @Override
        public boolean super_onKeyUp(int keyCode, KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEventPreIme(KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent event) {
            return false;
        }

        @Override
        public void super_onConfigurationChanged(Configuration newConfig) {
            if (mListener != null)
                mListener.onConfigurationChanged();
        }

        @Override
        public void onScrollChanged(int lPix, int tPix, int oldlPix, int oldtPix) {
        }

        @Override
        public boolean awakenScrollBars() {
            return false;
        }

        @Override
        public boolean super_awakenScrollBars(int startDelay, boolean invalidate) {
            return false;
        }
    }

    /**
     * Criteria used to know when an orientation change happens.
     */
    private static class OrientationChangeListenerCriteria implements Criteria {

        private boolean mSatisfied = false;

        private ConfigurationChangeAccessDelegate mAccessDelegate;

        private OrientationChangeListenerCriteria(
                ConfigurationChangeAccessDelegate accessDelegate) {
            mAccessDelegate = accessDelegate;
            mAccessDelegate.setListener(
                new ConfigurationChangeAccessDelegate.ConfigurationChangedListener() {
                    @Override
                    public void onConfigurationChanged() {
                        mSatisfied = true;
                    }
                });
        }

        @Override
        public boolean isSatisfied() {
            if (mSatisfied) {
                mAccessDelegate.setListener(null);
                return true;
            }

            return false;
        }
    }

    private ContentView mContentView;

    private ConfigurationChangeAccessDelegate mConfChangeAccessDelegate;

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
            new OrientationChangeListenerCriteria(mConfChangeAccessDelegate);

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

        mConfChangeAccessDelegate = new ConfigurationChangeAccessDelegate();
        getContentViewCore().
                setContainerViewInternals(mConfChangeAccessDelegate);
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
        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertEquals(90, getWindowOrientation());

        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        assertEquals(0, getWindowOrientation());

        lockOrientationAndWait(
            ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        assertEquals(-90, getWindowOrientation());

        lockOrientationAndWait(
            ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        int orientation = getWindowOrientation();
        assertTrue(orientation == 0 || orientation == 180);
    }
}
