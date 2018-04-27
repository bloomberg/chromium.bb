// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.view.ViewStub;

import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryModel.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

import javax.annotation.Nullable;

/**
 * Observes {@link KeyboardAccessoryModel} changes (like a newly available tab) and triggers the
 * {@link KeyboardAccessoryViewBinder} which will modify the view accordingly.
 */
class KeyboardAccessoryViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<KeyboardAccessoryModel,
                KeyboardAccessoryViewBinder.AccessoryViewHolder, PropertyKey> {
    public static class AccessoryViewHolder {
        @Nullable
        private KeyboardAccessoryView mView; // Remains null until |mViewStub| is inflated.
        private final ViewStub mViewStub;

        AccessoryViewHolder(ViewStub viewStub) {
            mViewStub = viewStub;
        }

        @Nullable
        public KeyboardAccessoryView getView() {
            return mView;
        }

        public void initializeView() {
            mView = (KeyboardAccessoryView) mViewStub.inflate();
        }
    }

    @Override
    public void bind(
            KeyboardAccessoryModel model, AccessoryViewHolder viewHolder, PropertyKey propertyKey) {
        KeyboardAccessoryView view = viewHolder.getView();
        if (view != null) { // If the view was previously inflated, update it and return.
            updateViewByProperty(model, view, propertyKey);
            return;
        }
        if (propertyKey != PropertyKey.VISIBLE || !model.isVisible()) {
            return; // Ignore model changes before the view is shown for the first time.
        }

        // If the view is visible for the first time, update ALL its properties.
        viewHolder.initializeView();
        for (PropertyKey key : PropertyKey.ALL_PROPERTIES) {
            updateViewByProperty(model, viewHolder.getView(), key);
        }
    }

    private void updateViewByProperty(
            KeyboardAccessoryModel model, KeyboardAccessoryView view, PropertyKey propertyKey) {
        if (propertyKey == PropertyKey.VISIBLE) {
            view.setVisible(model.isVisible());
            return;
        }
        if (propertyKey == PropertyKey.SUGGESTIONS) {
            view.updateSuggestions(model.getAutofillSuggestions());
            return;
        }
        assert false : "Every possible property update needs to be handled!";
    }
}
