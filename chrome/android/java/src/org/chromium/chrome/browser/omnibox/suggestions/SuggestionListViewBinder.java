// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.base.StrictModeContext;
import org.chromium.base.Supplier;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsList.OmniboxSuggestionListEmbedder;
import org.chromium.ui.UiUtils;

/**
 * Handles property updates to the suggestion list component.
 */
class SuggestionListViewBinder {
    /**
     * Holds the view components needed to renderer the suggestion list.
     */
    public static class SuggestionListViewHolder {
        public final Context context;
        public final Supplier<ViewStub> containerStubSupplier;
        public final OmniboxResultsAdapter adapter;

        OmniboxSuggestionsList mListView;
        ViewGroup mContainer;

        public SuggestionListViewHolder(Context context, Supplier<ViewStub> containerStubSupplier,
                OmniboxResultsAdapter adapter) {
            this.context = context;
            this.containerStubSupplier = containerStubSupplier;
            this.adapter = adapter;
        }
    }

    /**
     * @see
     * org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor.ViewBinder#bind(Object,
     * Object, Object)
     */
    public static void bind(
            PropertyModel model, SuggestionListViewHolder view, PropertyKey propertyKey) {
        if (SuggestionListProperties.VISIBLE.equals(propertyKey)) {
            boolean visible = model.get(SuggestionListProperties.VISIBLE);
            if (visible) {
                if (view.mContainer == null) {
                    view.mContainer = (ViewGroup) view.containerStubSupplier.get().inflate();
                }
                initializeSuggestionList(view, model);
                view.mContainer.setVisibility(View.VISIBLE);
                if (view.mListView.getParent() == null) view.mContainer.addView(view.mListView);
                view.mListView.show();
            } else {
                if (view.mContainer == null) return;
                view.mListView.setVisibility(View.GONE);
                UiUtils.removeViewFromParent(view.mListView);
                view.mContainer.setVisibility(View.INVISIBLE);
            }
        } else if (SuggestionListProperties.EMBEDDER.equals(propertyKey)) {
            if (view.mListView == null) return;
            view.mListView.setEmbedder(model.get(SuggestionListProperties.EMBEDDER));
        } else if (SuggestionListProperties.SUGGESTION_MODELS.equals(propertyKey)) {
            view.adapter.updateSuggestions(model.get(SuggestionListProperties.SUGGESTION_MODELS));
        } else if (SuggestionListProperties.USE_DARK_BACKGROUND.equals(propertyKey)) {
            if (view.mListView == null) return;
            view.mListView.refreshPopupBackground(
                    model.get(SuggestionListProperties.USE_DARK_BACKGROUND));
        }
    }

    private static void initializeSuggestionList(
            SuggestionListViewHolder view, PropertyModel model) {
        if (view.mListView != null) return;

        // TODO(tedchoc): Investigate lazily building the suggestion list off of the UI thread.
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            view.mListView = new OmniboxSuggestionsList(view.context);
        }

        // Start with visibility GONE to ensure that show() is called. http://crbug.com/517438
        view.mListView.setVisibility(View.GONE);
        view.mListView.setAdapter(view.adapter);
        view.mListView.setClipToPadding(false);

        OmniboxSuggestionListEmbedder embedder = model.get(SuggestionListProperties.EMBEDDER);
        if (embedder != null) view.mListView.setEmbedder(embedder);

        view.mListView.refreshPopupBackground(
                model.get(SuggestionListProperties.USE_DARK_BACKGROUND));
    }
}
