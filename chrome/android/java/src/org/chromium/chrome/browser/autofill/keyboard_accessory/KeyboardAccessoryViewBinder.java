// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Tab;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryModel.PropertyKey;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.ListModelChangeProcessor;

/**
 * Observes {@link KeyboardAccessoryModel} changes (like a newly available tab) and triggers the
 * {@link KeyboardAccessoryViewBinder} which will modify the view accordingly.
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

    static class TabViewBinder
            implements ListModelChangeProcessor.ViewBinder<ListModel<Tab>, KeyboardAccessoryView> {
        @Override
        public void onItemsInserted(
                ListModel<Tab> model, KeyboardAccessoryView view, int index, int count) {
            assert count > 0 : "Tried to insert invalid amount of tabs - must be at least one.";
            for (int i = index; i < index + count; i++) {
                Tab tab = model.get(i);
                view.addTabAt(i, tab.getIcon(), tab.getContentDescription());
            }
        }

        @Override
        public void onItemsRemoved(
                ListModel<Tab> model, KeyboardAccessoryView view, int index, int count) {
            assert count > 0 : "Tried to remove invalid amount of tabs - must be at least one.";
            while (count-- > 0) {
                view.removeTabAt(index++);
            }
        }

        @Override
        public void onItemsChanged(
                ListModel<Tab> model, KeyboardAccessoryView view, int index, int count) {
            // TODO(fhorschig): Implement fine-grained, ranged changes should the need arise.
            updateAllTabs(view, model);
        }

        void updateAllTabs(KeyboardAccessoryView view, ListModel<Tab> model) {
            view.clearTabs();
            if (model.size() > 0) onItemsInserted(model, view, 0, model.size());
        }
    }

    public static void bind(KeyboardAccessoryModel model, KeyboardAccessoryView view,
            PropertyKey propertyKey) {
        if (propertyKey == PropertyKey.ACTIONS) {
            view.setActionsAdapter(
                    KeyboardAccessoryCoordinator.createActionsAdapter(model.getActionList()));
        } else if (propertyKey == PropertyKey.TABS) {
            KeyboardAccessoryCoordinator.createTabViewBinder(model, view)
                    .updateAllTabs(view, model.getTabList());
        } else if (propertyKey == PropertyKey.VISIBLE) {
            view.setActiveTabColor(model.activeTab());
            view.setVisible(model.isVisible());
        } else if (propertyKey == PropertyKey.ACTIVE_TAB) {
            view.setActiveTabColor(model.activeTab());
        } else if (propertyKey == PropertyKey.BOTTOM_OFFSET) {
            view.setBottomOffset(model.bottomOffset());
        } else if (propertyKey == PropertyKey.TAB_SELECTION_CALLBACKS) {
            // Don't add null as listener. It's a valid state but an invalid argument.
            if (model.getTabSelectionCallbacks() == null) return;
            view.setTabSelectionAdapter(model.getTabSelectionCallbacks());
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }
}
