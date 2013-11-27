// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import com.google.common.annotations.VisibleForTesting;

/**
 * Allows monitoring of application menu actions.
 */
public interface AppMenuObserver {
    /**
     * Informs when the App Menu visibility changes.
     * @param newState Whether the menu is now visible.
     * @param focusedPosition The position that is currently in focus.
     */
    public void onMenuVisibilityChanged(boolean newState, int focusedPosition);

    /**
     * Notifies that the keyboard focus has changed within the App Menu.
     * @param focusedPosition The position that is currently in focus.
     */
    @VisibleForTesting
    public void onKeyboardFocusChanged(int focusedPosition);

    /**
     * Notifies that the keyboard has activated an item in the App Menu.
     * @param focusedPosition The position of the item that was activated.
     */
    @VisibleForTesting
    public void onKeyboardActivatedItem(int focusedPosition);
}
