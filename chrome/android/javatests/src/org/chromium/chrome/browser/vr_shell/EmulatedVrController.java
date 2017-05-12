// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.content.Context;
import android.os.SystemClock;

import com.google.vr.testframework.controller.ControllerTestApi;

import java.util.concurrent.TimeUnit;

/**
 * Wrapper for the ControllerTestApi class to handle more complex actions such
 * as clicking and dragging.
 *
 * Requires that VrCore's settings file is modified to use the test API:
 *   - UseAutomatedController: true
 *   - PairedControllerDriver: "DRIVER_AUTOMATED"
 *   - PairedControllerAddress: "FOO"
 */
public class EmulatedVrController {
    private final ControllerTestApi mApi;

    public EmulatedVrController(Context context) {
        mApi = new ControllerTestApi(context);
    }

    public ControllerTestApi getApi() {
        return mApi;
    }

    /**
     * Presses and quickly releases the Daydream controller's touchpad button.
     * Or, if the button is already pressed, releases and quickly presses again.
     */
    public void pressReleaseTouchpadButton() {
        mApi.buttonEvent.sendClickButtonEvent();
    }

    /**
     * Holds the home button to recenter the view using an arbitrary, but valid
     * orientation quaternion.
     */
    public void recenterView() {
        mApi.buttonEvent.sendHomeButtonToggleEvent();
        // A valid position must be sent a short time after the home button
        // is pressed in order for recentering to actually complete, and no
        // way to be signalled that we should send the event, so sleep
        SystemClock.sleep(500);
        // We don't care where the controller is pointing when recentering occurs as long
        // as it results in a successful recenter, so send an arbitrary, valid orientation
        mApi.moveEvent.sendMoveEvent(0.0f, 0.0f, 0.0f, 1.0f);
        mApi.buttonEvent.sendHomeButtonToggleEvent();
    }

    /**
     * Performs a short home button press/release, which launches the Daydream Home app.
     */
    public void goToDaydreamHome() {
        mApi.buttonEvent.sendShortHomeButtonEvent();
    }

    /**
     * Simulates a touch down-drag-touch up sequence on the touchpad between two points.
     *
     * @param xStart the x coordinate to start the touch sequence at, in range [0.0f, 1.0f]
     * @param yStart the y coordinate to start the touch sequence at, in range [0.0f, 1.0f]
     * @param xEnd the x coordinate to end the touch sequence at, in range [0.0f, 1.0f]
     * @param yEnd the y coordinate to end the touch sequence at, in range [0.0f, 1.0f]
     * @param steps the number of steps the drag will have
     * @param speed how long to wait between steps in the sequence. Generally, higher numbers
     * result in faster movement, e.g. when used for scrolling, a higher number results in faster
     * scrolling.
     */
    public void performLinearTouchpadMovement(
            float xStart, float yStart, float xEnd, float yEnd, int steps, int speed) {
        // Touchpad events have timestamps attached to them in nanoseconds - for smooth scrolling,
        // the timestamps should increase at a similar rate to the amount of time we actually wait
        // between sending events, which is determined by the given speed.
        long simulatedDelay = TimeUnit.MILLISECONDS.toNanos(speed);
        long timestamp = mApi.touchEvent.startTouchSequence(xStart, yStart, simulatedDelay, speed);
        timestamp = mApi.touchEvent.dragFromTo(
                xStart, yStart, xEnd, yEnd, steps, timestamp, simulatedDelay, speed);
        mApi.touchEvent.endTouchSequence(xEnd, yEnd, timestamp, simulatedDelay, speed);
    }

    /**
     * Performs an upward swipe on the touchpad, which scrolls down in VR Shell.
     * Note that scrolling this way is not consistent, i.e. scrolling down then
     * scrolling up at the same speed won't necessarily scroll back to the exact
     * starting position on the page.
     *
     * @param steps the number of intermediate steps to send while scrolling
     * @param speed how long to wait between steps in the scroll, with higher
     * numbers resulting in a faster scroll
     */
    public void scrollDown(int steps, int speed) {
        performLinearTouchpadMovement(0.5f, 0.9f, 0.5f, 0.1f, steps, speed);
    }

    /**
     * Performs a downward swipe on the touchpad, which scrolls up in VR Shell.
     * Note that scrolling this way is not consistent, i.e. scrolling down then
     * scrolling up at the same speed won't necessarily scroll back to the exact
     * starting position on the page.
     *
     * @param steps the number of intermediate steps to send while scrolling
     * @param speed how long to wait between steps in the scroll, with higher
     * numbers resulting in a faster scroll
     */
    public void scrollUp(int steps, int speed) {
        performLinearTouchpadMovement(0.5f, 0.1f, 0.5f, 0.9f, steps, speed);
    }

    /**
     * Instantly moves the controller to the specified quaternion coordinates.
     *
     * @param x the x component of the quaternion
     * @param y the y component of the quaternion
     * @param z the z component of the quaternion
     * @param w the w component of the quaternion
     */
    public void moveControllerInstant(float x, float y, float z, float w) {
        mApi.moveEvent.sendMoveEvent(x, y, z, w, 0);
    }

    /**
     * Moves the controller from one position to another over a period of time.
     *
     * @param startAngles the x/y/z angles to start the controller at, in radians
     * @param endAngles the x/y/z angles to end the controller at, in radians
     * @param steps the number of steps the controller will take moving between the positions
     * @param delayBetweenSteps how long to sleep between positions
     */
    public void moveControllerInterpolated(
            float[] startAngles, float[] endAngles, int steps, int delayBetweenSteps) {
        if (startAngles.length != 3 || endAngles.length != 3) {
            throw new IllegalArgumentException("Angle arrays must be length 3");
        }
        mApi.moveEvent.sendMoveEvent(new float[] {startAngles[0], endAngles[0]},
                new float[] {startAngles[1], endAngles[1]},
                new float[] {startAngles[2], endAngles[2]}, steps, delayBetweenSteps);
    }

    // TODO(bsheedy): Add support for more complex actions, e.g. click/drag/release
}
