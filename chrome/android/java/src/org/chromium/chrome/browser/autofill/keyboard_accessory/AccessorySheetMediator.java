// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import org.chromium.base.VisibleForTesting;

/**
 * Contains the controller logic of the AccessorySheet component.
 * It communicates with data providers and native backends to update a {@link AccessorySheetModel}.
 */
class AccessorySheetMediator {
    private final AccessorySheetModel mModel;
    private KeyboardAccessoryData.Tab mTab;

    AccessorySheetMediator(AccessorySheetModel model) {
        mModel = model;
    }

    KeyboardAccessoryData.Tab getTab() {
        return null; // TODO(fhorschig): Return the active tab.
    }

    @VisibleForTesting
    AccessorySheetModel getModelForTesting() {
        return mModel;
    }

    public void show() {
        mModel.setVisible(true);
    }

    public void hide() {
        mModel.setVisible(false);
    }
}