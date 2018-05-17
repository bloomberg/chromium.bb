// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

// TODO(fhorschig): Inline this class if the setting up/destroying is less overhead than expected.
// TODO(fhorschig): Otherwise, replace the KeyboardAccessorySheet in the ChromeActivity.
/**
 * Handles requests to the manual UI for filling passwords, payments and other user data. Ideally,
 * the caller has no access to Keyboard accessory or sheet and is only interacting with this
 * component.
 * For that, it facilitates the communication between {@link KeyboardAccessoryCoordinator} and
 * {@link AccessorySheetCoordinator} to add and trigger surfaces that may assist users while filling
 * fields.
 */
public class ManualFillingController {
    private final KeyboardAccessoryCoordinator mKeyboardAccessoryCoordinator;

    /**
     * Creates a the manual filling controller.
     * @param keyboardAccessoryCoordinator The keybaord
     */
    public ManualFillingController(KeyboardAccessoryCoordinator keyboardAccessoryCoordinator) {
        mKeyboardAccessoryCoordinator = keyboardAccessoryCoordinator;
    }

    /**
     * Links an {@link AccessorySheetCoordinator} to the {@link KeyboardAccessoryCoordinator}.
     * @param accessorySheetCoordinator The sheet component to be linked.
     */
    public void addAccessorySheet(AccessorySheetCoordinator accessorySheetCoordinator) {
        mKeyboardAccessoryCoordinator.addTab(accessorySheetCoordinator.getTab());
    }
}