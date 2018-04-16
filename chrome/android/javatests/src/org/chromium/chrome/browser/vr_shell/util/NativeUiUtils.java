// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell.util;

import android.graphics.Point;

import org.chromium.chrome.browser.vr_shell.TestVrShellDelegate;
import org.chromium.chrome.browser.vr_shell.VrUiTestAction;

/**
 * Class containing utility functions for interacting with the VR browser native UI, e.g. the
 * omnibox or back button.
 */
public class NativeUiUtils {
    /**
     * Clicks on a UI element as if done via a controller click.
     */
    public static void clickElement(int elementName) {
        TestVrShellDelegate instance = TestVrShellDelegate.getInstance();
        Point position = new Point(0, 0);
        instance.performUiActionForTesting(elementName, VrUiTestAction.HOVER_ENTER, position);
        instance.performUiActionForTesting(elementName, VrUiTestAction.BUTTON_DOWN, position);
        instance.performUiActionForTesting(elementName, VrUiTestAction.BUTTON_UP, position);
    }
}