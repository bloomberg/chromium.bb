// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.base.test.util.Feature;

import java.util.Arrays;
import java.util.BitSet;

/**
 * Verify no regressions in gamepad mappings.
 */
public class GamepadMappingsTest extends InstrumentationTestCase {
    /**
     * Set bits indicate that we don't expect the button at mMappedButtons[index] to be mapped.
     */
    private BitSet mUnmappedButtons = new BitSet(CanonicalButtonIndex.COUNT);
    /**
     * Set bits indicate that we don't expect the axis at mMappedAxes[index] to be mapped.
     */
    private BitSet mUnmappedAxes = new BitSet(CanonicalAxisIndex.COUNT);
    private float[] mMappedButtons = new float[CanonicalButtonIndex.COUNT];
    private float[] mMappedAxes = new float[CanonicalAxisIndex.COUNT];
    private float[] mRawButtons = new float[GamepadDevice.MAX_RAW_BUTTON_VALUES];
    private float[] mRawAxes = new float[GamepadDevice.MAX_RAW_AXIS_VALUES];

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // By default, we expect every button and axis to be mapped.
        mUnmappedButtons.clear();
        mUnmappedAxes.clear();

        // Start with all the mapped values as unmapped.
        Arrays.fill(mMappedButtons, Float.NaN);
        Arrays.fill(mMappedAxes, Float.NaN);

        // Set each raw value to something unique.
        for (int i = 0; i < GamepadDevice.MAX_RAW_AXIS_VALUES; i++) {
            mRawAxes[i] = -i - 1.0f;
        }
        for (int i = 0; i < GamepadDevice.MAX_RAW_BUTTON_VALUES; i++) {
            mRawButtons[i] = i + 1.0f;
        }
    }

    @SmallTest
    @Feature({"Gamepad"})
    public void testShieldGamepadMappings() throws Exception {
        GamepadMappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons,
                GamepadMappings.NVIDIA_SHIELD_DEVICE_NAME_PREFIX);

        assertShieldGamepadMappings();
    }

    @SmallTest
    @Feature({"Gamepad"})
    public void testXBox360GamepadMappings() throws Exception {
        GamepadMappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons,
                GamepadMappings.MICROSOFT_XBOX_PAD_DEVICE_NAME);

        assertShieldGamepadMappings();
    }

    @SmallTest
    @Feature({"Gamepad"})
    public void testPS3SixAxisGamepadMappings() throws Exception {
        GamepadMappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons,
                GamepadMappings.PS3_SIXAXIS_DEVICE_NAME);

        assertEquals(mMappedButtons[CanonicalButtonIndex.PRIMARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_X]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.SECONDARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_Y]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.TERTIARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_A]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.QUATERNARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_B]);

        assertMappedCommonTriggerButtons();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonDpadButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedTriggerAxexToShoulderButtons();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();

        assertMapping();
    }

    @SmallTest
    @Feature({"Gamepad"})
    public void testSamsungEIGP20GamepadMappings() throws Exception {
        GamepadMappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons,
                GamepadMappings.SAMSUNG_EI_GP20_DEVICE_NAME);

        assertMappedCommonXYABButtons();
        assertMappedCommonTriggerButtons();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedRXAndRYAxesToRightStick();

        expectNoShoulderButtons();
        assertMapping();
    }

    @SmallTest
    @Feature({"Gamepad"})
    public void testAmazonFireGamepadMappings() throws Exception {
        GamepadMappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons,
                GamepadMappings.AMAZON_FIRE_DEVICE_NAME);

        assertMappedCommonXYABButtons();
        assertMappedPedalAxesToBottomShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();

        assertMapping();
    }

    @SmallTest
    @Feature({"Gamepad"})
    public void testUnknownGamepadMappings() throws Exception {
        GamepadMappings.mapToStandardGamepad(
                mMappedAxes, mMappedButtons, mRawAxes, mRawButtons, "");

        assertMappedCommonXYABButtons();
        assertMappedCommonTriggerButtons();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedTriggerAxexToShoulderButtons();
        assertMappedCommonDpadButtons();
        assertMappedXYAxes();
        assertMappedRXAndRYAxesToRightStick();

        assertMapping();
    }

    /**
     * Asserts that the current gamepad mapping being tested matches the shield mappings.
     */
    public void assertShieldGamepadMappings() {
        assertMappedCommonXYABButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedTriggerAxesToBottomShoulder();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();

        assertMapping();
    }

    public void expectNoShoulderButtons() {
        mUnmappedButtons.set(CanonicalButtonIndex.LEFT_SHOULDER);
        mUnmappedButtons.set(CanonicalButtonIndex.RIGHT_SHOULDER);
    }

    public void assertMapping() {
        for (int i = 0; i < mMappedAxes.length; i++) {
            if (mUnmappedAxes.get(i)) {
                assertTrue(
                        "An unexpected axis was mapped at index " + i, Float.isNaN(mMappedAxes[i]));
            } else {
                assertFalse("An axis was not mapped at index " + i, Float.isNaN(mMappedAxes[i]));
            }
        }
        for (int i = 0; i < mMappedButtons.length; i++) {
            if (mUnmappedButtons.get(i)) {
                assertTrue("An unexpected button was mapped at index " + i,
                        Float.isNaN(mMappedButtons[i]));
            } else {
                assertFalse(
                        "A button was not mapped at index " + i, Float.isNaN(mMappedButtons[i]));
            }
        }
    }

    private void assertMappedCommonTriggerButtons() {
        assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_L1]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_R1]);
    }

    private void assertMappedCommonDpadButtons() {
        assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_DOWN],
                mRawButtons[KeyEvent.KEYCODE_DPAD_DOWN]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_UP],
                mRawButtons[KeyEvent.KEYCODE_DPAD_UP]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_LEFT],
                mRawButtons[KeyEvent.KEYCODE_DPAD_LEFT]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_RIGHT],
                mRawButtons[KeyEvent.KEYCODE_DPAD_RIGHT]);
    }

    private void assertMappedTriggerAxexToShoulderButtons() {
        assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_SHOULDER],
                mRawAxes[MotionEvent.AXIS_LTRIGGER]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER],
                mRawAxes[MotionEvent.AXIS_RTRIGGER]);
    }

    private void assertMappedTriggerButtonsToTopShoulder() {
        assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_SHOULDER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_L1]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_R1]);
    }

    private void assertMappedCommonXYABButtons() {
        assertEquals(mMappedButtons[CanonicalButtonIndex.PRIMARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_A]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.SECONDARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_B]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.TERTIARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_X]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.QUATERNARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_Y]);
    }

    private void assertMappedCommonThumbstickButtons() {
        assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_THUMBSTICK],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_THUMBL]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_THUMBSTICK],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_THUMBR]);
    }

    private void assertMappedCommonStartSelectMetaButtons() {
        assertEquals(mMappedButtons[CanonicalButtonIndex.START],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_START]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.BACK_SELECT],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_SELECT]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.META],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_MODE]);
    }

    private void assertMappedPedalAxesToBottomShoulder() {
        assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER],
                mRawAxes[MotionEvent.AXIS_BRAKE]);
        assertEquals(
                mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER], mRawAxes[MotionEvent.AXIS_GAS]);
    }

    private void assertMappedTriggerAxesToBottomShoulder() {
        assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER],
                mRawAxes[MotionEvent.AXIS_LTRIGGER]);
        assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER],
                mRawAxes[MotionEvent.AXIS_RTRIGGER]);
    }

    private void assertMappedHatAxisToDpadButtons() {
        float hatX = mRawAxes[MotionEvent.AXIS_HAT_X];
        float hatY = mRawAxes[MotionEvent.AXIS_HAT_Y];
        assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_LEFT],
                GamepadMappings.negativeAxisValueAsButton(hatX));
        assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_RIGHT],
                GamepadMappings.positiveAxisValueAsButton(hatX));
        assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_UP],
                GamepadMappings.negativeAxisValueAsButton(hatY));
        assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_DOWN],
                GamepadMappings.positiveAxisValueAsButton(hatY));
    }

    private void assertMappedXYAxes() {
        assertEquals(mMappedAxes[CanonicalAxisIndex.LEFT_STICK_X], mRawAxes[MotionEvent.AXIS_X]);
        assertEquals(mMappedAxes[CanonicalAxisIndex.LEFT_STICK_Y], mRawAxes[MotionEvent.AXIS_Y]);
    }

    private void assertMappedRXAndRYAxesToRightStick() {
        assertEquals(mMappedAxes[CanonicalAxisIndex.RIGHT_STICK_X], mRawAxes[MotionEvent.AXIS_RX]);
        assertEquals(mMappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y], mRawAxes[MotionEvent.AXIS_RY]);
    }

    private void assertMappedZAndRZAxesToRightStick() {
        assertEquals(mMappedAxes[CanonicalAxisIndex.RIGHT_STICK_X], mRawAxes[MotionEvent.AXIS_Z]);
        assertEquals(mMappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y], mRawAxes[MotionEvent.AXIS_RZ]);
    }
}
