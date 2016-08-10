// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.chrome.browser.ntp.snippets.CategoryStatus.CategoryStatusEnum;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleListItem;
import org.chromium.chrome.browser.ntp.snippets.SnippetHeaderListItem;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A group of suggestions, with a header, a status card, and a progress indicator.
 */
public class SuggestionsSection implements ItemGroup {
    private final List<SnippetArticleListItem> mSuggestions = new ArrayList<>();
    private final SuggestionsCategoryInfo mInfo;
    private final SnippetHeaderListItem mHeader;
    private StatusListItem mStatus;
    private final ProgressListItem mProgressIndicator = new ProgressListItem();
    private final ActionListItem mMoreButton;

    public SuggestionsSection(List<SnippetArticleListItem> suggestions,
            @CategoryStatusEnum int status, SuggestionsCategoryInfo info,
            NewTabPageAdapter adapter) {
        mInfo = info;
        mHeader = new SnippetHeaderListItem(mInfo.getTitle());
        mMoreButton = null; // TODO(pke): Read from info => ? new ActionListItem() : null;
        setSuggestions(suggestions, status, adapter);
    }

    @Override
    public List<NewTabPageListItem> getItems() {
        List<NewTabPageListItem> items = new ArrayList<>();
        items.add(mHeader);
        items.addAll(mSuggestions);
        if (mSuggestions.isEmpty()) {
            items.add(mStatus);
            items.add(mProgressIndicator);
        } else if (mMoreButton != null) {
            items.add(mMoreButton);
        }
        return Collections.unmodifiableList(items);
    }

    public void dismissSuggestion(SnippetArticleListItem suggestion) {
        mSuggestions.remove(suggestion);

        if (mSuggestions.isEmpty()) {
            mHeader.setVisible(false);
        }
    }

    public boolean hasSuggestions() {
        return !mSuggestions.isEmpty();
    }

    public void setSuggestions(List<SnippetArticleListItem> suggestions,
            @CategoryStatusEnum int status, NewTabPageAdapter adapter) {
        copyThumbnails(suggestions);

        mHeader.setVisible(!suggestions.isEmpty());

        mStatus = StatusListItem.create(status, adapter);
        mProgressIndicator.setVisible(SnippetsBridge.isCategoryLoading(status));

        mSuggestions.clear();
        mSuggestions.addAll(suggestions);
    }

    private void copyThumbnails(List<SnippetArticleListItem> suggestions) {
        for (SnippetArticleListItem suggestion : suggestions) {
            int index = mSuggestions.indexOf(suggestion);
            if (index == -1) continue;

            suggestion.setThumbnailBitmap(mSuggestions.get(index).getThumbnailBitmap());
        }
    }
}
