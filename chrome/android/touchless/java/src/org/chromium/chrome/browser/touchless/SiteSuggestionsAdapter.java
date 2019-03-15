// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import static org.chromium.chrome.browser.touchless.SiteSuggestionsController.SUGGESTIONS_KEY;

import android.content.Context;

import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.ui.modelutil.ForwardingListObservable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * Recycler view adapter for Touchless Suggestions carousel.
 *
 * Allows for an infinite side-scrolling carousel with snapping focus in the center.
 */
public class SiteSuggestionsAdapter
        extends ForwardingListObservable<Void> implements RecyclerViewAdapter.Delegate<
                SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder, Void> {
    private PropertyModel mModel;
    private RoundedIconGenerator mIconGenerator;
    private Context mContext;

    public SiteSuggestionsAdapter(
            PropertyModel model, RoundedIconGenerator iconGenerator, Context ctx) {
        mModel = model;
        mIconGenerator = iconGenerator;
        mContext = ctx;

        mModel.get(SUGGESTIONS_KEY).addObserver(this);
    }

    @Override
    public int getItemCount() {
        if (mModel.get(SUGGESTIONS_KEY).size() > 0) {
            return Integer.MAX_VALUE;
        }
        return 0;
    }

    @Override
    public int getItemViewType(int position) {
        return 0;
    }

    @Override
    public void onBindViewHolder(SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder holder,
            int position, Void payload) {
        SiteSuggestion item =
                mModel.get(SUGGESTIONS_KEY).get(position % mModel.get(SUGGESTIONS_KEY).size());
        SiteSuggestionsTileView image = (SiteSuggestionsTileView) holder.itemView;
        image.initialize(mIconGenerator);
        image.updateIcon(null, item.title);
    }
}
