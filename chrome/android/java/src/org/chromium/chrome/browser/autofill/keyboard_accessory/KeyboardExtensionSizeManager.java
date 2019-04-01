// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.annotation.Px;

/**
 * This class holds the size of any extension to or even replacement for a keyboard. The height can
 * be used to either compute an offset for bottom bars (e.g. CCTs or PWAs) or to push up the content
 * area.
 */
public interface KeyboardExtensionSizeManager {
    /**
     * Observers are notified when the size of the keyboard extension changes.
     */
    interface Observer {
        /**
         * Called when the extension height changes.
         * @param keyboardHeight The new height of the keyboard extension.
         */
        void onKeyboardExtensionHeightChanged(@Px int keyboardHeight);
    }

    /**
     * Returns the height of the keyboard extension.
     * @return A height in pixels.
     */
    @Px
    int getKeyboardExtensionHeight();

    /**
     * Registered observers are called whenever the extension size changes until unregistered.
     * Does not guarantee order.
     * @param observer a {@link KeyboardExtensionSizeManager.Observer}.
     */
    void addObserver(Observer observer);

    /**
     * Removes a registered observer if present.
     * @param observer a registered {@link KeyboardExtensionSizeManager.Observer}.
     */
    void removeObserver(Observer observer);
}