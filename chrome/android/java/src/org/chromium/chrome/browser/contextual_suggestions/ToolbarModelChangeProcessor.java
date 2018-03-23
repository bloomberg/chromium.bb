// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import org.chromium.chrome.browser.modelutil.PropertyObservable;
import org.chromium.chrome.browser.modelutil.PropertyObservable.PropertyObserver;

/**
 * A model change processor for use with a {@link ToolbarView}. The
 * {@link ToolbarModelChangeProcessor} should be registered as a property observer of
 * {@link ContextualSuggestionsModel}. Internally uses a view binder to bind model
 * properties to the toolbar view.
 */
class ToolbarModelChangeProcessor implements PropertyObserver<PropertyKey> {
    private static class ViewBinder {
        /**
         * Bind the changed property to the toolbar view.
         * @param view The {@link ToolbarView} to which data will be bound.
         * @param model The {@link ContextualSuggestionsModel} containing the data to be bound.
         * @param propertyKey The {@link PropertyKey} of the property that has changed.
         */
        private static void bindProperty(
                ToolbarView view, ContextualSuggestionsModel model, PropertyKey propertyKey) {
            switch (propertyKey.mKey) {
                case PropertyKey.CLOSE_BUTTON_ON_CLICK_LISTENER:
                    view.setOnClickListener(model.getCloseButtonOnClickListener());
                    break;
                case PropertyKey.TITLE:
                    view.setTitle(model.getTitle());
                    break;
                default:
                    assert false;
            }
        }
    }

    private final ToolbarView mToolbarView;
    private final ContextualSuggestionsModel mModel;

    /**
     * Construct a new ToolbarModelChangeProcessor.
     * @param view The {@link ToolbarView} to which data will be bound.
     * @param model The {@link ContextualSuggestionsModel} containing the data to be bound.
     */
    ToolbarModelChangeProcessor(ToolbarView view, ContextualSuggestionsModel model) {
        mToolbarView = view;
        mModel = model;

        // The ToolbarCoordinator is created dynamically as needed, so the initial model state
        // needs to be bound on creation.
        ViewBinder.bindProperty(
                mToolbarView, mModel, new PropertyKey(PropertyKey.CLOSE_BUTTON_ON_CLICK_LISTENER));
        ViewBinder.bindProperty(mToolbarView, mModel, new PropertyKey(PropertyKey.TITLE));
    }

    @Override
    public void onPropertyChanged(PropertyObservable<PropertyKey> source, PropertyKey propertyKey) {
        ViewBinder.bindProperty(mToolbarView, mModel, propertyKey);
    }
}
