// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.view.View.OnClickListener;

import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.PropertyObservable;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

import java.util.ArrayList;
import java.util.List;

/** A model for the contextual suggestions UI component. */
class ContextualSuggestionsModel extends PropertyObservable<PropertyKey> {
    /** A {@link ListObservable} containing the current list of suggestions. */
    class SuggestionsList extends ListObservable<SnippetArticle> {
        private List<SnippetArticle> mSuggestions = new ArrayList<>();

        private void setSuggestions(List<SnippetArticle> suggestions) {
            assert suggestions != null;

            int oldLength = getItemCount();
            mSuggestions = suggestions;

            if (oldLength != 0) notifyItemRangeRemoved(0, oldLength);
            if (mSuggestions.size() != 0) {
                notifyItemRangeInserted(0, mSuggestions.size());
            }
        }

        @Override
        public int getItemCount() {
            return mSuggestions.size();
        }

        @Override
        public SnippetArticle getItem(int position) {
            assert position < mSuggestions.size();

            return mSuggestions.get(position);
        }
    }

    SuggestionsList mSuggestionsList = new SuggestionsList();
    private OnClickListener mCloseButtonOnClickListener;

    /**
     * @param suggestions The list of current suggestions. May be an empty list if no
     *                    suggestions are available.
     */
    void setSuggestions(List<SnippetArticle> suggestions) {
        mSuggestionsList.setSuggestions(suggestions);
    }

    /** @return The list of current suggestions. */
    List<SnippetArticle> getSuggestions() {
        return mSuggestionsList.mSuggestions;
    }

    /** @param listener The {@link OnClickListener} for the close button. */
    void setCloseButtonOnClickListener(OnClickListener listener) {
        mCloseButtonOnClickListener = listener;
        notifyPropertyChanged(new PropertyKey(PropertyKey.ON_CLICK_LISTENER_PROPERTY));
    }

    /** @return The {@link OnClickListener} for the close button. */
    OnClickListener getCloseButtonOnClickListener() {
        return mCloseButtonOnClickListener;
    }
}
