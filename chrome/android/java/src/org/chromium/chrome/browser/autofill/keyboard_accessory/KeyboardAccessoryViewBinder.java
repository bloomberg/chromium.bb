// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.ACTIONS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.BOTTOM_OFFSET_PX;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.KEYBOARD_TOGGLE_VISIBLE;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.SHOW_KEYBOARD_CALLBACK;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.VISIBLE;

import android.os.Build;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Observes {@link KeyboardAccessoryProperties} changes (like a newly available tab) and triggers
 * the {@link KeyboardAccessoryViewBinder} which will modify the view accordingly.
 */
class KeyboardAccessoryViewBinder {
    static class ActionViewHolder extends RecyclerView.ViewHolder {
        public ActionViewHolder(View actionView) {
            super(actionView);
        }

        public static ActionViewHolder create(ViewGroup parent, @AccessoryAction int viewType) {
            switch (viewType) {
                case AccessoryAction.GENERATE_PASSWORD_AUTOMATIC:
                    return new ActionViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.keyboard_accessory_action, parent, false));
                case AccessoryAction.AUTOFILL_SUGGESTION:
                    return new ActionViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.keyboard_accessory_chip, parent, false));
                case AccessoryAction.MANAGE_PASSWORDS: // Intentional fallthrough.
                case AccessoryAction.COUNT:
                    assert false : "Type " + viewType + " is not a valid accessory bar action!";
            }
            assert false : "Action type " + viewType + " was not handled!";
            return null;
        }

        public void bind(Action action) {
            getView().setText(action.getCaption());
            getView().setOnClickListener(view -> action.getCallback().onResult(action));
        }

        private TextView getView() {
            return (TextView) super.itemView;
        }
    }

    public static void bind(
            PropertyModel model, KeyboardAccessoryView view, PropertyKey propertyKey) {
        boolean wasBound = bindInternal(model, view, propertyKey);
        assert wasBound : "Every possible property update needs to be handled!";
        requestLayoutPreKitkat(view);
    }

    /**
     * Tries to bind the given property to the given view by using the value in the given model.
     * @param model       A {@link PropertyModel}.
     * @param view        A {@link KeyboardAccessoryView}.
     * @param propertyKey A {@link PropertyKey}.
     * @return True if the given propertyKey was bound to the given view.
     */
    protected static boolean bindInternal(
            PropertyModel model, KeyboardAccessoryView view, PropertyKey propertyKey) {
        if (propertyKey == ACTIONS) {
            view.setActionsAdapter(
                    KeyboardAccessoryCoordinator.createActionsAdapter(model.get(ACTIONS)));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == BOTTOM_OFFSET_PX) {
            view.setBottomOffset(model.get(BOTTOM_OFFSET_PX));
        } else if (propertyKey == SHOW_KEYBOARD_CALLBACK) {
            // No binding required.
        } else if (propertyKey == KEYBOARD_TOGGLE_VISIBLE) {
            // No binding required.
        } else {
            return false;
        }
        return true;
    }

    protected static void requestLayoutPreKitkat(View view) {
        // Layout requests happen automatically since Kitkat and redundant requests cause warnings.
        if (view != null && Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
            view.post(() -> {
                ViewParent parent = view.getParent();
                if (parent != null) {
                    parent.requestLayout();
                }
            });
        }
    }
}
