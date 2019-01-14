// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.KEYBOARD_TOGGLE_VISIBLE;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.SHOW_KEYBOARD_CALLBACK;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.BarItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Observes {@link KeyboardAccessoryProperties} changes (like a newly available tab) and triggers
 * the {@link KeyboardAccessoryViewBinder} which will modify the view accordingly.
 */
class KeyboardAccessoryModernViewBinder {
    static class ModernBarItemViewHolder extends KeyboardAccessoryViewBinder.BarItemViewHolder {
        public ModernBarItemViewHolder(View barItemView) {
            super(barItemView);
        }

        public static KeyboardAccessoryViewBinder.BarItemViewHolder create(
                ViewGroup parent, @BarItem.Type int viewType) {
            switch (viewType) {
                case BarItem.Type.SUGGESTION:
                    return new ModernBarItemViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(
                                            R.layout.keyboard_accessory_suggestion, parent, false));
                case BarItem.Type.TAB_SWITCHER:
                    return new ModernBarItemViewHolder((
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.keyboard_accessory_tabs, parent, false)));
                case BarItem.Type.ACTION_BUTTON: // Intentional fallthrough. Use legacy handling.
                case BarItem.Type.COUNT: // Intentional fallthrough. Use legacy handling.
                    break;
            }
            return KeyboardAccessoryViewBinder.BarItemViewHolder.create(parent, viewType);
        }

        @Override
        public void bind(BarItem barItem) {
            super.bind(barItem);
        }
    }

    public static void bind(
            PropertyModel model, KeyboardAccessoryView view, PropertyKey propertyKey) {
        assert view instanceof KeyboardAccessoryModernView;
        KeyboardAccessoryModernView modernView = (KeyboardAccessoryModernView) view;
        boolean wasBound = KeyboardAccessoryViewBinder.bindInternal(model, modernView, propertyKey);
        if (propertyKey == KEYBOARD_TOGGLE_VISIBLE) {
            modernView.setKeyboardToggleVisibility(model.get(KEYBOARD_TOGGLE_VISIBLE));
        } else if (propertyKey == SHOW_KEYBOARD_CALLBACK) {
            modernView.setShowKeyboardCallback(model.get(SHOW_KEYBOARD_CALLBACK));
        } else {
            assert wasBound : "Every possible property update needs to be handled!";
        }
        KeyboardAccessoryViewBinder.requestLayoutPreKitkat(modernView);
    }
}
