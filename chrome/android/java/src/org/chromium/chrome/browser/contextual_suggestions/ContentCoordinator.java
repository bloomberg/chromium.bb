// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.RecyclerViewModelChangeProcessor;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

/**
 * Coordinator for the content sub-component. Responsible for communication with the parent
 * {@link ContextualSuggestionsCoordinator} and lifecycle of component objects.
 */
class ContentCoordinator {
    private final ContextualSuggestionsModel mModel;

    private SuggestionsRecyclerView mRecyclerView;
    private RecyclerViewModelChangeProcessor<SnippetArticle, ContextualSuggestionCardViewHolder>
            mModelChangeProcessor;

    /**
     * Construct a new {@link ContentCoordinator}.
     * @param context The {@link Context} used to retrieve resources.
     * @param parentView The parent {@link View} to which the content will eventually be attached.
     * @param profile The regular {@link Profile}.
     * @param uiDelegate The {@link SuggestionsUiDelegate} used to help construct items in the
     *                   content view.
     * @param model The {@link ContextualSuggestionsModel} for the component.
     */
    ContentCoordinator(Context context, ViewGroup parentView, Profile profile,
            SuggestionsUiDelegate uiDelegate, ContextualSuggestionsModel model) {
        mModel = model;

        mRecyclerView = (SuggestionsRecyclerView) LayoutInflater.from(context).inflate(
                R.layout.contextual_suggestions_layout, parentView, false);

        ContextualSuggestionsAdapter adapter = new ContextualSuggestionsAdapter(
                context, profile, new UiConfig(mRecyclerView), uiDelegate, mModel);
        mRecyclerView.setAdapter(adapter);

        mModelChangeProcessor = new RecyclerViewModelChangeProcessor<>(adapter);
        mModel.addObserver(mModelChangeProcessor);
    }

    /** @return The content {@link View}. */
    View getView() {
        return mRecyclerView;
    }

    /** @return The vertical scroll offset of the content view. */
    int getVerticalScrollOffset() {
        return mRecyclerView.computeVerticalScrollOffset();
    }

    /** Destroy the content component. */
    void destroy() {
        if (mRecyclerView == null) return;

        mRecyclerView.setAdapter(null);
        mRecyclerView = null;

        mModel.removeObserver(mModelChangeProcessor);
        mModelChangeProcessor = null;
    }
}
