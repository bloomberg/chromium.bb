// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;

/**
 * Class to manage mapping information related to each supported gamepad controller device.
 */
@JNINamespace("content")
class GamepadMappings {
    @VisibleForTesting
    static final String NVIDIA_SHIELD_DEVICE_NAME_PREFIX = "NVIDIA Corporation NVIDIA Controller";
    @VisibleForTesting
    static final String MICROSOFT_XBOX_PAD_DEVICE_NAME = "Microsoft X-Box 360 pad";
    @VisibleForTesting
    static final String PS3_SIXAXIS_DEVICE_NAME = "Sony PLAYSTATION(R)3 Controller";
    @VisibleForTesting
    static final String SAMSUNG_EI_GP20_DEVICE_NAME = "Samsung Game Pad EI-GP20";
    @VisibleForTesting
    static final String AMAZON_FIRE_DEVICE_NAME = "Amazon Fire Game Controller";

    public static boolean mapToStandardGamepad(float[] mappedAxes, float[] mappedButtons,
            float[] rawAxes, float[] rawButtons, String deviceName) {
        if (deviceName.startsWith(NVIDIA_SHIELD_DEVICE_NAME_PREFIX)) {
            mapShieldGamepad(mappedButtons, rawButtons, mappedAxes, rawAxes);
            return true;
        } else if (deviceName.equals(MICROSOFT_XBOX_PAD_DEVICE_NAME)) {
            mapXBox360Gamepad(mappedButtons, rawButtons, mappedAxes, rawAxes);
            return true;
        } else if (deviceName.equals(PS3_SIXAXIS_DEVICE_NAME)) {
            mapPS3SixAxisGamepad(mappedButtons, rawButtons, mappedAxes, rawAxes);
            return true;
        } else if (deviceName.equals(SAMSUNG_EI_GP20_DEVICE_NAME)) {
            mapSamsungEIGP20Gamepad(mappedButtons, rawButtons, mappedAxes, rawAxes);
            return true;
        } else if (deviceName.equals(AMAZON_FIRE_DEVICE_NAME)) {
            mapAmazonFireGamepad(mappedButtons, rawButtons, mappedAxes, rawAxes);
            return true;
        }

        mapUnknownGamepad(mappedButtons, rawButtons, mappedAxes, rawAxes);
        return false;
    }

    private static void mapCommonXYABButtons(float[] mappedButtons, float[] rawButtons) {
        float a = rawButtons[KeyEvent.KEYCODE_BUTTON_A];
        float b = rawButtons[KeyEvent.KEYCODE_BUTTON_B];
        float x = rawButtons[KeyEvent.KEYCODE_BUTTON_X];
        float y = rawButtons[KeyEvent.KEYCODE_BUTTON_Y];
        mappedButtons[CanonicalButtonIndex.PRIMARY] = a;
        mappedButtons[CanonicalButtonIndex.SECONDARY] = b;
        mappedButtons[CanonicalButtonIndex.TERTIARY] = x;
        mappedButtons[CanonicalButtonIndex.QUATERNARY] = y;
    }

    private static void mapCommonStartSelectMetaButtons(
            float[] mappedButtons, float[] rawButtons) {
        float start = rawButtons[KeyEvent.KEYCODE_BUTTON_START];
        float select = rawButtons[KeyEvent.KEYCODE_BUTTON_SELECT];
        float mode = rawButtons[KeyEvent.KEYCODE_BUTTON_MODE];
        mappedButtons[CanonicalButtonIndex.START] = start;
        mappedButtons[CanonicalButtonIndex.BACK_SELECT] = select;
        mappedButtons[CanonicalButtonIndex.META] = mode;
    }

    private static void mapCommonThumbstickButtons(float[] mappedButtons, float[] rawButtons) {
        float thumbL = rawButtons[KeyEvent.KEYCODE_BUTTON_THUMBL];
        float thumbR = rawButtons[KeyEvent.KEYCODE_BUTTON_THUMBR];
        mappedButtons[CanonicalButtonIndex.LEFT_THUMBSTICK] = thumbL;
        mappedButtons[CanonicalButtonIndex.RIGHT_THUMBSTICK] = thumbR;
    }

    private static void mapCommonTriggerButtons(float[] mappedButtons, float[] rawButtons) {
        float l1 = rawButtons[KeyEvent.KEYCODE_BUTTON_L1];
        float r1 = rawButtons[KeyEvent.KEYCODE_BUTTON_R1];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = l1;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = r1;
    }

    private static void mapTriggerButtonsToTopShoulder(float[] mappedButtons, float[] rawButtons) {
        float l1 = rawButtons[KeyEvent.KEYCODE_BUTTON_L1];
        float r1 = rawButtons[KeyEvent.KEYCODE_BUTTON_R1];
        mappedButtons[CanonicalButtonIndex.LEFT_SHOULDER] = l1;
        mappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER] = r1;
    }

    private static void mapCommonDpadButtons(float[] mappedButtons, float[] rawButtons) {
        float dpadDown = rawButtons[KeyEvent.KEYCODE_DPAD_DOWN];
        float dpadUp = rawButtons[KeyEvent.KEYCODE_DPAD_UP];
        float dpadLeft = rawButtons[KeyEvent.KEYCODE_DPAD_LEFT];
        float dpadRight = rawButtons[KeyEvent.KEYCODE_DPAD_RIGHT];
        mappedButtons[CanonicalButtonIndex.DPAD_DOWN] = dpadDown;
        mappedButtons[CanonicalButtonIndex.DPAD_UP] = dpadUp;
        mappedButtons[CanonicalButtonIndex.DPAD_LEFT] = dpadLeft;
        mappedButtons[CanonicalButtonIndex.DPAD_RIGHT] = dpadRight;
    }

    private static void mapXYAxes(float[] mappedAxes, float[] rawAxes) {
        mappedAxes[CanonicalAxisIndex.LEFT_STICK_X] = rawAxes[MotionEvent.AXIS_X];
        mappedAxes[CanonicalAxisIndex.LEFT_STICK_Y] = rawAxes[MotionEvent.AXIS_Y];
    }

    private static void mapRXAndRYAxesToRightStick(float[] mappedAxes, float[] rawAxes) {
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_X] = rawAxes[MotionEvent.AXIS_RX];
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y] = rawAxes[MotionEvent.AXIS_RY];
    }

    private static void mapZAndRZAxesToRightStick(float[] mappedAxes, float[] rawAxes) {
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_X] = rawAxes[MotionEvent.AXIS_Z];
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y] = rawAxes[MotionEvent.AXIS_RZ];
    }

    private static void mapTriggerAxexToShoulderButtons(float[] mappedButtons, float[] rawAxes) {
        float lTrigger = rawAxes[MotionEvent.AXIS_LTRIGGER];
        float rTrigger = rawAxes[MotionEvent.AXIS_RTRIGGER];
        mappedButtons[CanonicalButtonIndex.LEFT_SHOULDER] = lTrigger;
        mappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER] = rTrigger;
    }

    private static void mapPedalAxesToBottomShoulder(float[] mappedButtons, float[] rawAxes) {
        float lTrigger = rawAxes[MotionEvent.AXIS_BRAKE];
        float rTrigger = rawAxes[MotionEvent.AXIS_GAS];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = lTrigger;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = rTrigger;
    }

    private static void mapTriggerAxesToBottomShoulder(float[] mappedButtons, float[] rawAxes) {
        float lTrigger = rawAxes[MotionEvent.AXIS_LTRIGGER];
        float rTrigger = rawAxes[MotionEvent.AXIS_RTRIGGER];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = lTrigger;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = rTrigger;
    }

    @VisibleForTesting
    static float negativeAxisValueAsButton(float input) {
        return (input < -0.5f) ? 1.f : 0.f;
    }

    @VisibleForTesting
    static float positiveAxisValueAsButton(float input) {
        return (input > 0.5f) ? 1.f : 0.f;
    }

    private static void mapHatAxisToDpadButtons(float[] mappedButtons, float[] rawAxes) {
        float hatX = rawAxes[MotionEvent.AXIS_HAT_X];
        float hatY = rawAxes[MotionEvent.AXIS_HAT_Y];
        mappedButtons[CanonicalButtonIndex.DPAD_LEFT] = negativeAxisValueAsButton(hatX);
        mappedButtons[CanonicalButtonIndex.DPAD_RIGHT] = positiveAxisValueAsButton(hatX);
        mappedButtons[CanonicalButtonIndex.DPAD_UP] = negativeAxisValueAsButton(hatY);
        mappedButtons[CanonicalButtonIndex.DPAD_DOWN] = positiveAxisValueAsButton(hatY);
    }

    /**
     * Method for mapping Amazon Fire gamepad axis and button values
     * to standard gamepad button and axes values.
     */
    private static void mapAmazonFireGamepad(
            float[] mappedButtons, float[] rawButtons, float[] mappedAxes, float[] rawAxes) {
        mapCommonXYABButtons(mappedButtons, rawButtons);
        mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
        mapCommonThumbstickButtons(mappedButtons, rawButtons);
        mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
        mapPedalAxesToBottomShoulder(mappedButtons, rawAxes);
        mapHatAxisToDpadButtons(mappedButtons, rawAxes);

        mapXYAxes(mappedAxes, rawAxes);
        mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
    }

    /**
     * Method for mapping Nvidia gamepad axis and button values
     * to standard gamepad button and axes values.
     */
    private static void mapShieldGamepad(float[] mappedButtons, float[] rawButtons,
            float[] mappedAxes, float[] rawAxes) {
        mapCommonXYABButtons(mappedButtons, rawButtons);
        mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
        mapCommonThumbstickButtons(mappedButtons, rawButtons);
        mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
        mapTriggerAxesToBottomShoulder(mappedButtons, rawAxes);
        mapHatAxisToDpadButtons(mappedButtons, rawAxes);

        mapXYAxes(mappedAxes, rawAxes);
        mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
    }

    /**
     * Method for mapping Microsoft XBox 360 gamepad axis and button values
     * to standard gamepad button and axes values.
     */
    private static void mapXBox360Gamepad(float[] mappedButtons, float[] rawButtons,
            float[] mappedAxes, float[] rawAxes) {
        // These are actually mapped the same way in Android.
        mapShieldGamepad(mappedButtons, rawButtons, mappedAxes, rawAxes);
    }

    private static void mapPS3SixAxisGamepad(float[] mappedButtons, float[] rawButtons,
            float[] mappedAxes, float[] rawAxes) {
        // On PS3 X/Y has higher priority.
        float a = rawButtons[KeyEvent.KEYCODE_BUTTON_A];
        float b = rawButtons[KeyEvent.KEYCODE_BUTTON_B];
        float x = rawButtons[KeyEvent.KEYCODE_BUTTON_X];
        float y = rawButtons[KeyEvent.KEYCODE_BUTTON_Y];
        mappedButtons[CanonicalButtonIndex.PRIMARY] = x;
        mappedButtons[CanonicalButtonIndex.SECONDARY] = y;
        mappedButtons[CanonicalButtonIndex.TERTIARY] = a;
        mappedButtons[CanonicalButtonIndex.QUATERNARY] = b;

        mapCommonTriggerButtons(mappedButtons, rawButtons);
        mapCommonThumbstickButtons(mappedButtons, rawButtons);
        mapCommonDpadButtons(mappedButtons, rawButtons);
        mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
        mapTriggerAxexToShoulderButtons(mappedButtons, rawAxes);

        mapXYAxes(mappedAxes, rawAxes);
        mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
    }

    private static void mapSamsungEIGP20Gamepad(float[] mappedButtons, float[] rawButtons,
            float[] mappedAxes, float[] rawAxes) {
        mapCommonXYABButtons(mappedButtons, rawButtons);
        mapCommonTriggerButtons(mappedButtons, rawButtons);
        mapCommonThumbstickButtons(mappedButtons, rawButtons);
        mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
        mapHatAxisToDpadButtons(mappedButtons, rawAxes);

        mapXYAxes(mappedAxes, rawAxes);
        mapRXAndRYAxesToRightStick(mappedAxes, rawAxes);
    }

    /**
     * Method for mapping Unkown gamepad axis and button values
     * to standard gamepad button and axes values.
     */
    private static void mapUnknownGamepad(float[] mappedButtons, float[] rawButtons,
            float[] mappedAxes, float[] rawAxes) {
        mapCommonXYABButtons(mappedButtons, rawButtons);
        mapCommonTriggerButtons(mappedButtons, rawButtons);
        mapCommonThumbstickButtons(mappedButtons, rawButtons);
        mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
        mapTriggerAxexToShoulderButtons(mappedButtons, rawAxes);
        mapCommonDpadButtons(mappedButtons, rawButtons);

        mapXYAxes(mappedAxes, rawAxes);
        mapRXAndRYAxesToRightStick(mappedAxes, rawAxes);
    }
}
