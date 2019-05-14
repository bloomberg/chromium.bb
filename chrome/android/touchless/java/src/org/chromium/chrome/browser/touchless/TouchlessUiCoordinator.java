// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.view.KeyEvent;

import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Coordinator for Touchless UI. */
public interface TouchlessUiCoordinator {
    /**
     * @param event The KeyEvent.
     * @return Whether the KeyEvent was handled by the Touchless UI.
     */
    boolean dispatchKeyEvent(KeyEvent event);

    /**
     * @return The SnackbarManager for touchless mode.
     */
    SnackbarManager getSnackbarManager();

    /**
     * @return The ModalDialogManager for touchless mode.
     */
    ModalDialogManager createModalDialogManager();
}
