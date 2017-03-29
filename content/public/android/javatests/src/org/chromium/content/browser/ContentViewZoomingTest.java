// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.SystemClock;
import android.support.test.filters.SmallTest;
import android.view.InputDevice;
import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.input.JoystickZoomProvider;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

/**
 * Tests that ContentView running inside ContentShell can be zoomed using gamepad joystick.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ContentViewZoomingTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String LARGE_PAGE = UrlUtils.encodeHtmlDataUri("<html><head>"
            + "<meta name=\"viewport\" content=\"width=device-width, "
            + "initial-scale=2.0, minimum-scale=2.0, maximum-scale=5.0\" />"
            + "<style>body { width: 5000px; height: 5000px; }</style></head>"
            + "<body>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</body>"
            + "</html>");

    private static class TestAnimationIntervalProvider
            implements JoystickZoomProvider.AnimationIntervalProvider {
        private long mAnimationTime;
        @Override
        public long getLastAnimationFrameInterval() {
            mAnimationTime += 16;
            return mAnimationTime;
        }
    }

    private class TestJoystickZoomProvider extends JoystickZoomProvider {
        TestJoystickZoomProvider(ContentViewCore cvc, AnimationIntervalProvider intervalProvider) {
            super(cvc.getContainerView(), 2.0f, cvc.getViewportWidthPix() / 2,
                    cvc.getViewportHeightPix() / 2, cvc);
            setAnimationIntervalProviderForTesting(intervalProvider);

            mZoomRunnable = new Runnable() {
                @Override
                public void run() {}
            };
        }

        public void animateZoomTest(final MotionEvent joystickZoomEvent, final long animationTicks)
                throws Throwable {
            mActivityTestRule.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    onMotion(joystickZoomEvent);
                    for (int index = 0; index < animationTicks; index++) {
                        animateZoom();
                    }
                    stop();
                }
            });
        }
    }

    private MotionEvent simulateJoystickEvent(final float delta, final boolean isZoomInRequest) {
        // Synthesize joystick motion event and send to ContentViewCore.
        final int axisVal =
                (isZoomInRequest) ? MotionEvent.AXIS_RTRIGGER : MotionEvent.AXIS_LTRIGGER;
        MotionEvent.PointerCoords[] cords = new MotionEvent.PointerCoords[1];
        MotionEvent.PointerProperties[] pPts = new MotionEvent.PointerProperties[1];
        cords[0] = new MotionEvent.PointerCoords();
        pPts[0] = new MotionEvent.PointerProperties();
        cords[0].setAxisValue(axisVal, delta);
        pPts[0].id = 0;
        MotionEvent joystickMotionEvent = MotionEvent.obtain((long) 0, SystemClock.uptimeMillis(),
                MotionEvent.ACTION_MOVE, 1, pPts, cords, 0, 0, 0.01f, 0.01f, 3, 0,
                InputDevice.SOURCE_CLASS_JOYSTICK, 0);
        return joystickMotionEvent;
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchContentShellWithUrl(LARGE_PAGE);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mActivityTestRule.assertWaitForPageScaleFactorMatch(2.0f);
    }

    @Test
    @SmallTest
    @Feature({"JoystickZoom"})
    public void testJoystickZoomIn() throws Throwable {
        MotionEvent rTriggerEvent;
        TestJoystickZoomProvider rtJoystickZoomProvider = new TestJoystickZoomProvider(
                mActivityTestRule.getContentViewCore(), new TestAnimationIntervalProvider());
        // Verify page does not zoom-in if trigger motion falls in deadzone.
        rTriggerEvent = simulateJoystickEvent(0.1f, true);
        rtJoystickZoomProvider.animateZoomTest(rTriggerEvent, 20);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(2.0f);

        rTriggerEvent = simulateJoystickEvent(0.3f, true);
        rtJoystickZoomProvider.animateZoomTest(rTriggerEvent, 20);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(2.2018466f);

        rTriggerEvent = simulateJoystickEvent(0.5f, true);
        rtJoystickZoomProvider.animateZoomTest(rTriggerEvent, 40);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(3.033731f);

        rTriggerEvent = simulateJoystickEvent(0.75f, true);
        rtJoystickZoomProvider.animateZoomTest(rTriggerEvent, 50);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(5.0f);
    }

    @Test
    @SmallTest
    @Feature({"JoystickZoom"})
    public void testJoystickZoomOut() throws Throwable {
        MotionEvent lTriggerEvent;
        TestJoystickZoomProvider ltJoystickZoomProvider = new TestJoystickZoomProvider(
                mActivityTestRule.getContentViewCore(), new TestAnimationIntervalProvider());

        // Zoom page to max size.
        lTriggerEvent = simulateJoystickEvent(1.0f, true);
        ltJoystickZoomProvider.animateZoomTest(lTriggerEvent, 60);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(5.0f);

        // Verify page does not zoom-out if trigger motion falls in deadzone.
        lTriggerEvent = simulateJoystickEvent(0.1f, false);
        ltJoystickZoomProvider.animateZoomTest(lTriggerEvent, 20);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(5.0f);

        lTriggerEvent = simulateJoystickEvent(0.3f, false);
        ltJoystickZoomProvider.animateZoomTest(lTriggerEvent, 40);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(4.125306f);

        lTriggerEvent = simulateJoystickEvent(0.5f, false);
        ltJoystickZoomProvider.animateZoomTest(lTriggerEvent, 50);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(2.7635581f);

        lTriggerEvent = simulateJoystickEvent(0.75f, false);
        ltJoystickZoomProvider.animateZoomTest(lTriggerEvent, 60);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(2.0f);
    }
}
