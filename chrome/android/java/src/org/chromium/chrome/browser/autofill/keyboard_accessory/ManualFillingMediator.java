// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.UiUtils;

/**
 * This part of the manual filling component manages the state of the manual filling flow depending
 * on the currently shown tab.
 */
class ManualFillingMediator implements KeyboardAccessoryCoordinator.VisibilityDelegate {
    private KeyboardAccessoryCoordinator mKeyboardAccessory;
    private AccessorySheetCoordinator mAccessorySheet;
    private ChromeActivity mActivity; // Used to control the keyboard.

    void initialize(KeyboardAccessoryCoordinator keyboardAccessory,
            AccessorySheetCoordinator accessorySheet, ChromeActivity activity) {
        mKeyboardAccessory = keyboardAccessory;
        mAccessorySheet = accessorySheet;
        mActivity = activity;
    }

    void destroy() {
        // TODO(fhorschig): Remove all tabs. Destroy all providers.
        // mPasswordAccessorySheet.destroy();
        mKeyboardAccessory.destroy();
    }

    /**
     * Links a tab to the manual UI by adding it to the held {@link AccessorySheetCoordinator} and
     * the {@link KeyboardAccessoryCoordinator}.
     * @param tab The tab component to be added.
     */
    void addTab(KeyboardAccessoryData.Tab tab) {
        // TODO(fhorschig): Should add Tabs per URL/chome tab.
        mKeyboardAccessory.addTab(tab);
        mAccessorySheet.addTab(tab);
    }

    void removeTab(KeyboardAccessoryData.Tab tab) {
        // TODO(fhorschig): Should remove Tabs per URL/chome tab.
        mKeyboardAccessory.removeTab(tab);
        mAccessorySheet.removeTab(tab);
    }

    @Override
    public void onChangeAccessorySheet(int tabIndex) {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mAccessorySheet.setActiveTab(tabIndex);
    }

    @Override
    public void onOpenAccessorySheet() {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        UiUtils.hideKeyboard(mActivity.getCurrentFocus());
        mAccessorySheet.show();
    }

    @Override
    public void onCloseAccessorySheet() {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mAccessorySheet.hide();
        UiUtils.showKeyboard(mActivity.getCurrentFocus());
    }

    @Override
    public void onCloseKeyboardAccessory() {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mAccessorySheet.hide();
        UiUtils.hideKeyboard(mActivity.getCurrentFocus());
    }

    // TODO(fhorschig): Should be @VisibleForTesting.
    AccessorySheetCoordinator getAccessorySheet() {
        return mAccessorySheet;
    }

    // TODO(fhorschig): Should be @VisibleForTesting.
    KeyboardAccessoryCoordinator getKeyboardAccessory() {
        return mKeyboardAccessory;
    }
}
