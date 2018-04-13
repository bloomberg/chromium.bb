// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import org.chromium.base.VisibleForTesting;

/**
 * Creates and owns all elements which are part of the keyboard accessory component.
 * It's part of the controller but will mainly forward events (like adding a tab,
 * or showing the accessory) to the {@link KeyboardAccessoryMediator}.
 */
public class KeyboardAccessoryCoordinator {
    private final KeyboardAccessoryMediator mMediator =
            new KeyboardAccessoryMediator(new KeyboardAccessoryModel());

    @VisibleForTesting
    KeyboardAccessoryMediator getMediatorForTesting() {
        return mMediator;
    }

    public void hide() {
        mMediator.hide();
    }

    public void show() {
        mMediator.show();
    }

    public void addTab(KeyboardAccessoryData.Tab tab) {
        mMediator.addTab(tab);
    }

    public void removeTab(KeyboardAccessoryData.Tab tab) {
        mMediator.removeTab(tab);
    }

    /**
     * Allows any {@link KeyboardAccessoryData.ActionListProvider} to communicate with the
     * {@link KeyboardAccessoryMediator} of this component.
     * @param provider The object providing action lists to observers in this component.
     */
    public void registerActionListProvider(KeyboardAccessoryData.ActionListProvider provider) {
        provider.addObserver(mMediator);
    }
}
