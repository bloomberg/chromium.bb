// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryModel.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.ui.widget.ButtonCompat;

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
        private final RecyclerViewAdapter<KeyboardAccessoryModel.SimpleListObservable<Action>,
                ActionViewBinder.ViewHolder> mActionsAdapter;

        AccessoryViewHolder(ViewStub viewStub,
                RecyclerViewAdapter<KeyboardAccessoryModel.SimpleListObservable<Action>,
                        ActionViewBinder.ViewHolder> actionsAdapter) {
            mViewStub = viewStub;
            mActionsAdapter = actionsAdapter;
            mActionsAdapter.setViewBinder(new ActionViewBinder());
        }

        @Nullable
        public KeyboardAccessoryView getView() {
            return mView;
        }

        public void initializeView() {
            mView = (KeyboardAccessoryView) mViewStub.inflate();
            mView.setActionsAdapter(mActionsAdapter);
        }
    }

    static class ActionViewBinder implements RecyclerViewAdapter.ViewBinder<
            KeyboardAccessoryModel.SimpleListObservable<Action>, ActionViewBinder.ViewHolder> {
        static class ViewHolder extends RecyclerView.ViewHolder {
            public ViewHolder(ButtonCompat actionView) {
                super(actionView);
            }

            ButtonCompat getActionView() {
                return (ButtonCompat) super.itemView;
            }
        }

        @Override
        public ActionViewBinder.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            return new ViewHolder(
                    (ButtonCompat) LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.keyboard_accessory_action, parent, false));
        }

        @Override
        public void onBindViewHolder(KeyboardAccessoryModel.SimpleListObservable<Action> actions,
                ViewHolder holder, int position) {
            final Action action = actions.get(position);
            holder.getActionView().setText(action.getCaption());
            holder.getActionView().setOnClickListener(
                    view -> action.getDelegate().onActionTriggered(action));
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
