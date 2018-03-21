// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

import java.util.ArrayList;
import java.util.List;

/** A model for the contextual suggestions UI component. */
class ContextualSuggestionsModel extends ListObservable<SnippetArticle> {
    private List<SnippetArticle> mSuggestions = new ArrayList<>();

    /**
     * @param suggestions The list of current suggestions. May be an empty list if no
     *                    suggestions are available.
     */
    void setSuggestions(List<SnippetArticle> suggestions) {
        assert suggestions != null;

        if (suggestions.size() == 0 && mSuggestions.size() == 0) return;

        int oldLength = getItemCount();
        mSuggestions = suggestions;

        if (oldLength != 0) notifyItemRangeRemoved(0, oldLength);
        if (mSuggestions.size() != 0) {
            notifyItemRangeInserted(0, mSuggestions.size());
        }
    }

    /** @return The list of current suggestions. */
    List<SnippetArticle> getSuggestions() {
        return mSuggestions;
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
