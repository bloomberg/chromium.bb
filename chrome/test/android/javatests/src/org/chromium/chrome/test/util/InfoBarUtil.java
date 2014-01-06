// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.test.ActivityInstrumentationTestCase2;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.content.browser.test.util.TouchCommon;

/**
 * Utility functions for dealing with InfoBars.
 */
public class InfoBarUtil {
    /**
     * Finds, and optionally clicks, the button with the specified ID in the given InfoBar.
     * @return True if the View was found.
     */
    private static boolean findButton(ActivityInstrumentationTestCase2<?> test,
            InfoBar infoBar, int buttonId, boolean click) {
        View button = infoBar.getContentWrapper().findViewById(buttonId);
        if (button == null) return false;
        if (click) new TouchCommon(test).singleClickView(button);
        return true;
    }

    /**
     * Checks if the primary button exists on the InfoBar.
     * @return True if the View was found.
     */
    public static boolean hasPrimaryButton(ActivityInstrumentationTestCase2<?> test,
            InfoBar infoBar) {
        return findButton(test, infoBar, R.id.button_primary, false);
    }

    /**
     * Checks if the secondary button exists on the InfoBar.
     * @return True if the View was found.
     */
    public static boolean hasSecondaryButton(ActivityInstrumentationTestCase2<?> test,
            InfoBar infoBar) {
        return findButton(test, infoBar, R.id.button_secondary, false);
    }

    /**
     * Simulates clicking the Close button in the specified infobar.
     * @return True if the View was found.
     */
    public static boolean clickCloseButton(ActivityInstrumentationTestCase2<?> test,
            InfoBar infoBar) {
        return findButton(test, infoBar, R.id.infobar_close_button, true);
    }

    /**
     * Simulates clicking the primary button in the specified infobar.
     * @return True if the View was found.
     */
    public static boolean clickPrimaryButton(ActivityInstrumentationTestCase2<?> test,
            InfoBar infoBar) {
        return findButton(test, infoBar, R.id.button_primary, true);
    }

    /**
     * Simulates clicking the secondary button in the specified infobar.
     * @return True if the View was found.
     */
    public static boolean clickSecondaryButton(ActivityInstrumentationTestCase2<?> test,
            InfoBar infoBar) {
        return findButton(test, infoBar, R.id.button_secondary, true);
    }
}
