// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.KEYBOARD_TOGGLE_VISIBLE;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.SHOW_KEYBOARD_CALLBACK;

import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryViewBinder.BarItemViewHolder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChipView;

/**
 * Observes {@link KeyboardAccessoryProperties} changes (like a newly available tab) and triggers
 * the {@link KeyboardAccessoryViewBinder} which will modify the view accordingly.
 */
class KeyboardAccessoryModernViewBinder {
    public static BarItemViewHolder create(ViewGroup parent, @BarItem.Type int viewType) {
        switch (viewType) {
            case BarItem.Type.SUGGESTION:
                return new BarItemChipViewHolder(parent);
            case BarItem.Type.TAB_SWITCHER:
                return new TabItemViewHolder(parent);
            case BarItem.Type.ACTION_BUTTON: // Intentional fallthrough. Use legacy handling.
            case BarItem.Type.COUNT: // Intentional fallthrough. Use legacy handling.
                break;
        }
        return KeyboardAccessoryViewBinder.create(parent, viewType);
    }

    static class BarItemChipViewHolder extends BarItemViewHolder<BarItem, ChipView> {
        BarItemChipViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_suggestion_modern);
        }

        @Override
        protected void bind(BarItem item, ChipView chipView) {
            KeyboardAccessoryData.Action action = item.getAction();
            assert action != null : "Tried to bind item without action. Chose a wrong ViewHolder?";
            chipView.getPrimaryTextView().setText(action.getCaption());
            chipView.setOnClickListener(view -> action.getCallback().onResult(action));
        }
    }

    static class TabItemViewHolder
            extends BarItemViewHolder<BarItem, KeyboardAccessoryTabLayoutView> {
        private KeyboardAccessoryTabLayoutView mTabLayout;

        TabItemViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_suggestion);
        }

        @Override
        protected void bind(BarItem item, KeyboardAccessoryTabLayoutView view) {}
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
