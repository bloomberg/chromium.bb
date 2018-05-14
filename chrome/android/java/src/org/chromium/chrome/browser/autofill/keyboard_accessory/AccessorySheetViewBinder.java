// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.view.View;

import org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetModel.PropertyKey;
import org.chromium.chrome.browser.modelutil.LazyViewBinderAdapter;

/**
 * Observes {@link AccessorySheetModel} changes (like a newly available tab) and triggers the
 * {@link AccessorySheetViewBinder} which will modify the view accordingly.
 */
class AccessorySheetViewBinder
        implements LazyViewBinderAdapter.SimpleViewBinder<AccessorySheetModel, View, PropertyKey> {
    @Override
    public PropertyKey getVisibilityProperty() {
        return PropertyKey.VISIBLE;
    }

    @Override
    public boolean isVisible(AccessorySheetModel model) {
        return model.isVisible();
    }

    @Override
    public void onInitialInflation(AccessorySheetModel model, View inflatedView) {}

    @Override
    public void bind(AccessorySheetModel model, View inflatedView, PropertyKey propertyKey) {
        if (propertyKey == PropertyKey.VISIBLE) {
            inflatedView.setVisibility(model.isVisible() ? View.VISIBLE : View.GONE);
            return;
        }
        assert false : "Every possible property update needs to be handled!";
    }
}
