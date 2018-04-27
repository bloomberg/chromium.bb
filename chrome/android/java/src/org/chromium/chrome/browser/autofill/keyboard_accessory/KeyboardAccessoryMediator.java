// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.autofill.AutofillKeyboardSuggestions;
import org.chromium.ui.base.WindowAndroid;

/**
 * This is the second part of the controller of the keyboard accessory component.
 * It is responsible to update the {@link KeyboardAccessoryModel} based on Backend calls and notify
 * the Backend if the {@link KeyboardAccessoryModel} changes.
 * From the backend, it receives all actions that the accessory can perform (most prominently
 * generating passwords) and lets the {@link KeyboardAccessoryModel} know of these actions and which
 * callback to trigger when selecting them.
 */
class KeyboardAccessoryMediator implements KeyboardAccessoryData.ActionListObserver,
                                           WindowAndroid.KeyboardVisibilityListener {
    private final KeyboardAccessoryModel mModel;
    private final WindowAndroid mWindowAndroid;

    KeyboardAccessoryMediator(KeyboardAccessoryModel model, WindowAndroid windowAndroid) {
        mModel = model;
        mWindowAndroid = windowAndroid;
        windowAndroid.addKeyboardVisibilityListener(this);
    }

    void destroy() {
        mWindowAndroid.removeKeyboardVisibilityListener(this);
    }

    @Override
    public void onActionsAvailable(KeyboardAccessoryData.Action[] actions) {
        mModel.setActions(actions);
    }

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        if (!isShowing) { // TODO(fhorschig): ... and no bottom sheet.
            hide();
        }
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

    void setSuggestions(AutofillKeyboardSuggestions suggestions) {
        mModel.setAutofillSuggestions(suggestions);
    }

    void dismiss() {
        hide();
        if (mModel.getAutofillSuggestions() != null) {
            mModel.getAutofillSuggestions().dismiss();
        }
        mModel.setAutofillSuggestions(null);
    }

    @VisibleForTesting
    KeyboardAccessoryModel getModelForTesting() {
        return mModel;
    }
}
