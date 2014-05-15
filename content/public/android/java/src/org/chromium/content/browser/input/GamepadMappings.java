// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.view.KeyEvent;

/**
 * Class to manage mapping information related to each supported gamepad controller device.
 */
class GamepadMappings {
    public static boolean mapToStandardGamepad(float[] mappedAxis, float[] mappedButtons,
            float[] rawAxes, float[] rawButtons, String deviceName) {
        if (deviceName.contains("NVIDIA Corporation NVIDIA Controller")) {
            mapShieldGamepad(mappedButtons, rawButtons, mappedAxis, rawAxes);
            return true;
        } else if (deviceName.contains("Microsoft X-Box 360 pad")) {
            mapXBox360Gamepad(mappedButtons, rawButtons, mappedAxis, rawAxes);
            return true;
        }

        mapUnknownGamepad(mappedButtons, rawButtons, mappedAxis, rawAxes);
        return false;
    }

    private static void mapCommonButtons(float[] mappedButtons, float[] rawButtons) {
        mappedButtons[CanonicalButtonIndex.BUTTON_PRIMARY] = rawButtons[KeyEvent.KEYCODE_BUTTON_A];
        mappedButtons[CanonicalButtonIndex.BUTTON_SECONDARY] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_B];
        mappedButtons[CanonicalButtonIndex.BUTTON_TERTIARY] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_X];
        mappedButtons[CanonicalButtonIndex.BUTTON_QUATERNARY] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_Y];
        mappedButtons[CanonicalButtonIndex.BUTTON_LEFT_SHOULDER] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_L1];
        mappedButtons[CanonicalButtonIndex.BUTTON_RIGHT_SHOULDER] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_R1];
        mappedButtons[CanonicalButtonIndex.BUTTON_BACK_SELECT] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_SELECT];
        mappedButtons[CanonicalButtonIndex.BUTTON_START] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_START];
        mappedButtons[CanonicalButtonIndex.BUTTON_LEFT_THUMBSTICK] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_THUMBL];
        mappedButtons[CanonicalButtonIndex.BUTTON_RIGHT_THUMBSTICK] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_THUMBR];
        mappedButtons[CanonicalButtonIndex.BUTTON_META] = rawButtons[KeyEvent.KEYCODE_BUTTON_MODE];
    }

    private static void mapDpadButtonsToAxes(float[] mappedButtons, float[] rawAxes) {
        // Negative value indicates dpad up.
        mappedButtons[CanonicalButtonIndex.BUTTON_DPAD_UP] = negativeAxisValueAsButton(rawAxes[9]);
        // Positive axis value indicates dpad down.
        mappedButtons[CanonicalButtonIndex.BUTTON_DPAD_DOWN] =
                positiveAxisValueAsButton(rawAxes[9]);
        // Positive axis value indicates dpad right.
        mappedButtons[CanonicalButtonIndex.BUTTON_DPAD_RIGHT] =
                positiveAxisValueAsButton(rawAxes[8]);
        // Negative value indicates dpad left.
        mappedButtons[CanonicalButtonIndex.BUTTON_DPAD_LEFT] =
                negativeAxisValueAsButton(rawAxes[8]);
    }

    private static void mapAxes(float[] mappedAxis, float[] rawAxes) {
        // Standard gamepad can have only four axes.
        mappedAxis[CanonicalAxisIndex.AXIS_LEFT_STICK_X] = rawAxes[0];
        mappedAxis[CanonicalAxisIndex.AXIS_LEFT_STICK_Y] = rawAxes[1];
        mappedAxis[CanonicalAxisIndex.AXIS_RIGHT_STICK_X] = rawAxes[4];
        mappedAxis[CanonicalAxisIndex.AXIS_RIGHT_STICK_Y] = rawAxes[5];
    }

    private static float negativeAxisValueAsButton(float input) {
        return (input < -0.5f) ? 1.f : 0.f;
    }

    private static float positiveAxisValueAsButton(float input) {
        return (input > 0.5f) ? 1.f : 0.f;
    }

    /**
     * Method for mapping Nvidia gamepad axis and button values
     * to standard gamepad button and axes values.
     */
    private static void mapShieldGamepad(float[] mappedButtons, float[] rawButtons,
            float[] mappedAxis, float[] rawAxes) {
        mapCommonButtons(mappedButtons, rawButtons);

        mappedButtons[CanonicalButtonIndex.BUTTON_LEFT_TRIGGER] = rawAxes[2];
        mappedButtons[CanonicalButtonIndex.BUTTON_RIGHT_TRIGGER] = rawAxes[6];

        mapDpadButtonsToAxes(mappedButtons, rawAxes);
        mapAxes(mappedAxis, rawAxes);
    }

    /**
     * Method for mapping Microsoft XBox 360 gamepad axis and button values
     * to standard gamepad button and axes values.
     */
    private static void mapXBox360Gamepad(float[] mappedButtons, float[] rawButtons,
            float[] mappedAxis, float[] rawAxes) {
        mapCommonButtons(mappedButtons, rawButtons);

        mappedButtons[CanonicalButtonIndex.BUTTON_LEFT_TRIGGER] = rawAxes[2];
        mappedButtons[CanonicalButtonIndex.BUTTON_RIGHT_TRIGGER] = rawAxes[6];

        mapDpadButtonsToAxes(mappedButtons, rawAxes);
        mapAxes(mappedAxis, rawAxes);
    }

    /**
     * Method for mapping Unkown gamepad axis and button values
     * to standard gamepad button and axes values.
     */
    private static void mapUnknownGamepad(float[] mappedButtons, float[] rawButtons,
            float[] mappedAxis, float[] rawAxes) {
        mapCommonButtons(mappedButtons, rawButtons);

        mappedButtons[CanonicalButtonIndex.BUTTON_LEFT_TRIGGER] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_L2];
        mappedButtons[CanonicalButtonIndex.BUTTON_RIGHT_TRIGGER] =
                rawButtons[KeyEvent.KEYCODE_BUTTON_R2];

        mappedButtons[CanonicalButtonIndex.BUTTON_DPAD_UP] = rawButtons[KeyEvent.KEYCODE_DPAD_UP];
        mappedButtons[CanonicalButtonIndex.BUTTON_DPAD_DOWN] =
                rawButtons[KeyEvent.KEYCODE_DPAD_DOWN];
        mappedButtons[CanonicalButtonIndex.BUTTON_DPAD_RIGHT] =
                rawButtons[KeyEvent.KEYCODE_DPAD_RIGHT];
        mappedButtons[CanonicalButtonIndex.BUTTON_DPAD_LEFT] =
                rawButtons[KeyEvent.KEYCODE_DPAD_LEFT];

        mapAxes(mappedAxis, rawAxes);
    }

}