// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.accessibility.FontSizePrefs;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.test.util.UiUtils;

import java.text.NumberFormat;

/**
 * Tests for the Settings menu.
 */
public class PreferencesTest extends ChromeShellTestBase {

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());
    }

    /**
     * Launches the preferences menu and starts the preferences activity named fragmentName.
     * Returns the activity that was started.
     */
    public static Preferences startPreferences(Instrumentation instrumentation,
            String fragmentName) {
        Context context = instrumentation.getTargetContext();
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(context, fragmentName);
        Activity activity = instrumentation.startActivitySync(intent);
        assertTrue(activity instanceof Preferences);
        return (Preferences) activity;
    }

    /**
     * Tests setting FontScaleFactor and ForceEnableZoom in AccessibilityPreferences and ensures
     * that ForceEnableZoom changes corresponding to FontScaleFactor.
     */
    @SmallTest
    @Feature({"Accessibility"})
    public void testAccessibilityPreferences() throws Exception {
        String accessibilityPrefClassname = AccessibilityPreferences.class.getName();
        AccessibilityPreferences accessibilityPref = (AccessibilityPreferences)
                startPreferences(getInstrumentation(), accessibilityPrefClassname)
                .getFragmentForTest();
        SeekBarPreference textScalePref = (SeekBarPreference) accessibilityPref.findPreference(
                AccessibilityPreferences.PREF_TEXT_SCALE);
        SeekBarLinkedCheckBoxPreference forceEnableZoomPref =
                (SeekBarLinkedCheckBoxPreference) accessibilityPref
                        .findPreference(AccessibilityPreferences.PREF_FORCE_ENABLE_ZOOM);
        NumberFormat percentFormat = NumberFormat.getPercentInstance();
        // Arbitrary value 0.4f to be larger and smaller than threshold.
        float fontSmallerThanThreshold =
                AccessibilityPreferences.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER - 0.4f;
        float fontBiggerThanThreshold =
                AccessibilityPreferences.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER + 0.4f;

        // Set the textScaleFactor above the threshold.
        userSetTextScale(accessibilityPref, textScalePref, fontBiggerThanThreshold);
        UiUtils.settleDownUI(getInstrumentation());
        // Since above the threshold, this will check the force enable zoom button.
        assertEquals(percentFormat.format(fontBiggerThanThreshold), textScalePref.getSummary());
        assertTrue(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(true, fontBiggerThanThreshold);

        // Set the textScaleFactor below the threshold.
        userSetTextScale(accessibilityPref, textScalePref, fontSmallerThanThreshold);
        UiUtils.settleDownUI(getInstrumentation());
        // Since below the threshold and userSetForceEnableZoom is false, this will uncheck
        // the force enable zoom button.
        assertEquals(percentFormat.format(fontSmallerThanThreshold), textScalePref.getSummary());
        assertFalse(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(false, fontSmallerThanThreshold);

        userSetTextScale(accessibilityPref, textScalePref, fontBiggerThanThreshold);
        // Sets onUserSetForceEnableZoom to be true.
        userSetForceEnableZoom(accessibilityPref, forceEnableZoomPref, true);
        UiUtils.settleDownUI(getInstrumentation());
        // Since userSetForceEnableZoom is true, when the text scale is moved below the threshold
        // ForceEnableZoom should remain checked.
        userSetTextScale(accessibilityPref, textScalePref, fontSmallerThanThreshold);
        assertTrue(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(true, fontSmallerThanThreshold);
    }

    private void assertFontSizePrefs(final boolean expectedForceEnableZoom,
            final float expectedFontScale) {
        final Context targetContext = getInstrumentation().getTargetContext();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FontSizePrefs fontSizePrefs = FontSizePrefs.getInstance(targetContext);
                assertEquals(expectedForceEnableZoom, fontSizePrefs.getForceEnableZoom());
                assertEquals(expectedFontScale, fontSizePrefs.getFontScaleFactor(), 0.001f);
            }
        });
    }

    private static void userSetTextScale(final AccessibilityPreferences accessibilityPref,
            final SeekBarPreference textScalePref, final float textScale) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                accessibilityPref.onPreferenceChange(textScalePref, textScale);
            }
        });
    }

    private static void userSetForceEnableZoom(final AccessibilityPreferences accessibilityPref,
            final SeekBarLinkedCheckBoxPreference forceEnableZoomPref, final boolean enabled) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                accessibilityPref.onPreferenceChange(forceEnableZoomPref, enabled);
            }
        });
    }
}
