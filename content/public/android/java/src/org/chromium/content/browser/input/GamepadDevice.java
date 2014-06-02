// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.SystemClock;
import android.view.InputDevice;
import android.view.InputDevice.MotionRange;
import android.view.KeyEvent;
import android.view.MotionEvent;

import java.util.Arrays;

/**
 * Manages information related to each connected gamepad device.
 */
class GamepadDevice {
    // An id for the gamepad.
    private int mDeviceId;
    // The index of the gamepad in the Navigator.
    private int mDeviceIndex;
    // Last time the data for this gamepad was updated.
    private long mTimestamp;
    // If this gamepad is mapped to standard gamepad?
    private boolean mIsStandardGamepad;

    // Array of values for all axes of the gamepad.
    // All axis values must be linearly normalized to the range [-1.0 .. 1.0].
    // As appropriate, -1.0 should correspond to "up" or "left", and 1.0
    // should correspond to "down" or "right".
    private float[] mAxisValues;

    private float[] mButtonsValues;

    // When the user agent recognizes the attached inputDevice, it is recommended
    // that it be remapped to a canonical ordering when possible. Devices that are
    // not recognized should still be exposed in their raw form. Therefore we must
    // pass the raw Button and raw Axis values.
    private float[] mRawButtons;
    private float[] mRawAxes;

    // An identification string for the gamepad.
    private String mDeviceName;

    // Array of axes ids.
    private int[] mAxes;

    GamepadDevice(int index, InputDevice inputDevice) {
        mDeviceIndex = index;
        mDeviceId = inputDevice.getId();
        mDeviceName = inputDevice.getName();
        mTimestamp = SystemClock.uptimeMillis();
        mButtonsValues = new float[CanonicalButtonIndex.NUM_CANONICAL_BUTTONS];
        mAxisValues = new float[CanonicalAxisIndex.NUM_CANONICAL_AXES];
        mRawButtons = new float[256];
        // Get the total number of axes supported by gamepad.
        int totalAxes = 0;
        for (MotionRange range : inputDevice.getMotionRanges()) {
            if ((range.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {
                totalAxes++;
            }
        }
        // Get axis ids and initialize axes values.
        mAxes = new int[totalAxes];
        mRawAxes = new float[totalAxes];
        int i = 0;
        for (MotionRange range : inputDevice.getMotionRanges()) {
            if ((range.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {
                mAxes[i] = range.getAxis();
                mRawAxes[i] = 0.0f;
                i++;
            }
        }
    }

    /**
     * Updates the axes and buttons maping of a gamepad device to a standard gamepad format.
     */
    public void updateButtonsAndAxesMapping() {
        mIsStandardGamepad = GamepadMappings.mapToStandardGamepad(
                mAxisValues, mButtonsValues, mRawAxes, mRawButtons, mDeviceName);
    }

    /**
     * @return Device Id of the gamepad device.
     */
    public int getId() { return mDeviceId; }

    /**
     * @return Mapping status of the gamepad device.
     */
    public boolean isStandardGamepad() { return mIsStandardGamepad; }

    /**
     * @return Device name of the gamepad device.
     */
    public String getName() { return mDeviceName; }

    /**
     * @return Device index of the gamepad device.
     */
    public int getIndex() { return mDeviceIndex; }

    /**
     * @return The timestamp when the gamepad device was last interacted.
     */
    public long getTimestamp() { return mTimestamp; }

    /**
     * @return The axes state of the gamepad device.
     */
    public float[] getAxes() { return mAxisValues; }

    /**
     * @return The buttons state of the gamepad device.
     */
    public float[] getButtons() { return mButtonsValues; }

    /**
     * Reset the axes and buttons data of the gamepad device everytime gamepad data access is
     * paused.
     */
    public void clearData() {
        Arrays.fill(mAxisValues, 0);
        Arrays.fill(mRawAxes, 0);
        Arrays.fill(mButtonsValues, 0);
        Arrays.fill(mRawButtons, 0);
    }

    /**
     * Handles key event from the gamepad device.
     * @return True if the key event from the gamepad device has been consumed.
     */
    public boolean handleKeyEvent(KeyEvent event) {
        // Ignore event if it is not for standard gamepad key.
        if (!GamepadList.isGamepadEvent(event)) return false;
        int keyCode = event.getKeyCode();
        assert keyCode < 256;
        // Button value 0.0 must mean fully unpressed, and 1.0 must mean fully pressed.
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            mRawButtons[keyCode] = 1.0f;
        } else if (event.getAction() == KeyEvent.ACTION_UP) {
            mRawButtons[keyCode] = 0.0f;
        }
        mTimestamp = event.getEventTime();

        return true;
    }

    /**
     * Handles motion event from the gamepad device.
     * @return True if the motion event from the gamepad device has been consumed.
     */
    public boolean handleMotionEvent(MotionEvent event) {
        // Ignore event if it is not a standard gamepad motion event.
        if (!GamepadList.isGamepadEvent(event)) return false;
        // Update axes values.
        for (int i = 0; i < mAxes.length; i++) {
            mRawAxes[i] = event.getAxisValue(mAxes[i]);
        }
        mTimestamp = event.getEventTime();
        return true;
    }

}
