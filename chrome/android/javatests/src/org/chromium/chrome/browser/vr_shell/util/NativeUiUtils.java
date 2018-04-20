// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell.util;

import android.graphics.PointF;
import android.view.View;

import org.junit.Assert;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.vr_shell.TestVrShellDelegate;
import org.chromium.chrome.browser.vr_shell.UserFriendlyElementName;
import org.chromium.chrome.browser.vr_shell.VrDialog;
import org.chromium.chrome.browser.vr_shell.VrShellImpl;
import org.chromium.chrome.browser.vr_shell.VrUiTestAction;
import org.chromium.chrome.browser.vr_shell.VrViewContainer;

/**
 * Class containing utility functions for interacting with the VR browser native UI, e.g. the
 * omnibox or back button.
 */
public class NativeUiUtils {
    /**
     * Clicks on a UI element as if done via a controller click.
     */
    public static void clickElement(int elementName, PointF position) {
        TestVrShellDelegate instance = TestVrShellDelegate.getInstance();
        instance.performUiActionForTesting(elementName, VrUiTestAction.HOVER_ENTER, position);
        instance.performUiActionForTesting(elementName, VrUiTestAction.BUTTON_DOWN, position);
        instance.performUiActionForTesting(elementName, VrUiTestAction.BUTTON_UP, position);
    }

    /**
     * Clicks on a fallback UI element's positive button, e.g. "Allow" or "Confirm".
     */
    public static void clickFallbackUiPositiveButton() {
        clickFallbackUiButton(R.id.positive_button);
    }

    /**
     * Clicks on a fallback UI element's negative button, e.g. "Deny" or "Cancel".
     */
    public static void clickFallbackUiNegativeButton() {
        clickFallbackUiButton(R.id.negative_button);
    }

    private static void clickFallbackUiButton(int buttonId) {
        VrShellImpl vrShell = (VrShellImpl) (TestVrShellDelegate.getVrShellForTesting());
        VrViewContainer viewContainer = vrShell.getVrViewContainerForTesting();
        Assert.assertTrue(
                "VrViewContainer actually has children", viewContainer.getChildCount() > 0);
        // Click on whatever dialog was most recently added
        VrDialog vrDialog = (VrDialog) viewContainer.getChildAt(viewContainer.getChildCount() - 1);
        View button = vrDialog.findViewById(buttonId);
        Assert.assertNotNull("Found a View with matching ID", button);
        // We need to flip the Y axis since this calculates from the bottom left, but Android seems
        // to process input events from the top left?
        // We also need to scale by the VrDialog's width/height since the native code will later
        // multiply by the same amount.
        float x = (button.getX() + button.getWidth() / 2) / vrDialog.getWidth();
        float y = (vrDialog.getHeight() - (button.getY() + button.getHeight() / 2))
                / vrDialog.getHeight();
        PointF buttonCenter = new PointF(x, y);
        clickElement(UserFriendlyElementName.BROWSING_DIALOG, buttonCenter);
    }
}