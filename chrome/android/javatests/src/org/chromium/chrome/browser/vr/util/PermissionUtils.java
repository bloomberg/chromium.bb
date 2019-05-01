// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.permissions.PermissionDialogController;
import org.chromium.chrome.browser.vr.ArConsentDialog;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Utility class for interacting with permission prompts outside of the VR Browser. For interaction
 * in the VR Browser, see NativeUiUtils.
 */
public class PermissionUtils {
    /**
     * Blocks until a permission prompt appears.
     */
    public static void waitForPermissionPrompt() {
        CriteriaHelper.pollUiThread(() -> {
            return PermissionDialogController.getInstance().isDialogShownForTest();
        }, "Permission prompt did not appear in allotted time");
    }

    /**
     * Accepts the currently displayed permission prompt.
     */
    public static void acceptPermissionPrompt() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionDialogController.getInstance().clickButtonForTest(
                    ModalDialogProperties.ButtonType.POSITIVE);
        });
    }

    /**
     * Denies the currently displayed permission prompt.
     */
    public static void denyPermissionPrompt() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionDialogController.getInstance().clickButtonForTest(
                    ModalDialogProperties.ButtonType.NEGATIVE);
        });
    }

    /**
     * Blocks until the AR session consent prompt appears.
     */
    public static void waitForArConsentPrompt(ChromeActivity activity) {
        CriteriaHelper.pollUiThread(() -> {
            return isArConsentDialogShown(activity);
        }, "AR consent prompt did not appear in allotted time");
    }

    /**
     * Accepts the currently displayed AR session consent prompt.
     */
    public static void acceptArConsentPrompt(ChromeActivity activity) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            clickArConsentDialogButton(activity, ModalDialogProperties.ButtonType.POSITIVE);
        });
    }

    /**
     * Declines the currently displayed AR session consent prompt.
     */
    public static void declineArConsentPrompt(ChromeActivity activity) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            clickArConsentDialogButton(activity, ModalDialogProperties.ButtonType.NEGATIVE);
        });
    }

    /**
     * Helper function to check if the AR consent dialog is being shown.
     */
    public static boolean isArConsentDialogShown(ChromeActivity activity) {
        ModalDialogManager manager = activity.getModalDialogManager();
        PropertyModel model = manager.getCurrentDialogForTest();
        if (model == null) return false;
        return model.get(ModalDialogProperties.CONTROLLER) instanceof ArConsentDialog;
    }

    /**
     * Helper function to click a button in the AR consent dialog.
     */
    public static void clickArConsentDialogButton(ChromeActivity activity, int buttonType) {
        ModalDialogManager manager = activity.getModalDialogManager();
        PropertyModel model = manager.getCurrentDialogForTest();
        ArConsentDialog dialog = (ArConsentDialog) (model.get(ModalDialogProperties.CONTROLLER));
        dialog.onClick(model, buttonType);
    }
}
