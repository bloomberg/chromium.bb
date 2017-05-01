// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;

/**
 * The controller for translate UI snackbars.
 */
class TranslateSnackbarController implements SnackbarController {
    @Override
    public void onDismissNoAction(Object actionData) {
        // No action.
    }

    @Override
    public void onAction(Object actionData) {
        nativeToggleTranslateOption(((Integer) actionData).intValue());
    }

    private static native void nativeToggleTranslateOption(int type);
};
