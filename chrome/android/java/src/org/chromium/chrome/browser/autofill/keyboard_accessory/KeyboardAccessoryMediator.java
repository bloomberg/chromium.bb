// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import org.chromium.base.VisibleForTesting;

/**
 * This is the second part of the controller of the keyboard accessory component.
 * It is responsible to update the {@link KeyboardAccessoryModel} based on Backend calls and notify
 * the Backend if the {@link KeyboardAccessoryModel} changes.
 * From the backend, it receives all actions that the accessory can perform (most prominently
 * generating passwords) and lets the {@link KeyboardAccessoryModel} know of these actions and which
 * callback to trigger when selecting them.
 */
class KeyboardAccessoryMediator implements KeyboardAccessoryData.ActionListObserver {
    private final KeyboardAccessoryModel mModel;

    KeyboardAccessoryMediator(KeyboardAccessoryModel keyboardAccessoryModel) {
        mModel = keyboardAccessoryModel;
    }

    @Override
    public void onActionsAvailable(KeyboardAccessoryData.Action[] actions) {
        mModel.setActions(actions);
    }

    void hide() {
        mModel.setVisible(false);
    }

    void show() {
        mModel.setVisible(true);
    }

    void addTab(KeyboardAccessoryData.Tab tab) {
        mModel.addTab(tab);
    }

    void removeTab(KeyboardAccessoryData.Tab tab) {
        mModel.removeTab(tab);
    }

    @VisibleForTesting
    KeyboardAccessoryModel getModelForTesting() {
        return mModel;
    }
}
