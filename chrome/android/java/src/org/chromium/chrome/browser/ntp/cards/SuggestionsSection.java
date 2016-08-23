// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.cards.StatusItem.ActionDelegate;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus.CategoryStatusEnum;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A group of suggestions, with a header, a status card, and a progress indicator.
 */
public class SuggestionsSection implements ItemGroup {
    private final List<SnippetArticle> mSuggestions = new ArrayList<>();
    private final SectionHeader mHeader;
    private StatusItem mStatus;
    private final ProgressItem mProgressIndicator = new ProgressItem();
    private final ActionDelegate mActionDelegate;
    private final ActionItem mMoreButton;

    public SuggestionsSection(@CategoryInt int category, SuggestionsCategoryInfo info,
            final NewTabPageAdapter adapter) {
        mHeader = new SectionHeader(info.getTitle());
        // TODO(pke): Replace the condition with "info.hasMoreButton()" once all other categories
        // are supported by the C++ backend, too.
        // Right now, we hard-code all the sections that are handled in ActionListItem.
        boolean showMoreButton = false;
        if (category == KnownCategories.BOOKMARKS) {
            showMoreButton = true;
        } else if (category == KnownCategories.DOWNLOADS) {
            showMoreButton = ChromeFeatureList.isEnabled("DownloadsUi");
        }
        mMoreButton = showMoreButton ? new ActionItem(category) : null;

        // TODO(dgn): Properly define strings, actions, etc. for each section and category type.
        if (showMoreButton) {
            mActionDelegate = null;
        } else {
            mActionDelegate = new ActionDelegate() {
                @Override
                public void onButtonTapped() {
                    adapter.reloadSnippets();
                }
            };
        }
    }

    @Override
    public List<NewTabPageItem> getItems() {
        List<NewTabPageItem> items = new ArrayList<>();
        items.add(mHeader);
        items.addAll(mSuggestions);

        if (mSuggestions.isEmpty()) items.add(mStatus);
        if (mMoreButton != null) items.add(mMoreButton);
        if (mSuggestions.isEmpty()) items.add(mProgressIndicator);

        return Collections.unmodifiableList(items);
    }

    public void removeSuggestion(SnippetArticle suggestion) {
        mSuggestions.remove(suggestion);
    }

    public void removeSuggestionById(String suggestionId) {
        for (SnippetArticle suggestion : mSuggestions) {
            if (suggestion.mId.equals(suggestionId)) {
                removeSuggestion(suggestion);
                return;
            }
        }
    }

    public boolean hasSuggestions() {
        return !mSuggestions.isEmpty();
    }

    public int getSuggestionsCount() {
        return mSuggestions.size();
    }

    public void setSuggestions(List<SnippetArticle> suggestions, @CategoryStatusEnum int status) {
        copyThumbnails(suggestions);

        setStatus(status);

        mSuggestions.clear();
        mSuggestions.addAll(suggestions);
    }

    /** Sets the status for the section. Some statuses can cause the suggestions to be cleared. */
    public void setStatus(@CategoryStatusEnum int status) {
        mStatus = StatusItem.create(status, mActionDelegate);

        if (!SnippetsBridge.isCategoryStatusAvailable(status)) mSuggestions.clear();

        mProgressIndicator.setVisible(SnippetsBridge.isCategoryLoading(status));
    }

    private void copyThumbnails(List<SnippetArticle> suggestions) {
        for (SnippetArticle suggestion : suggestions) {
            int index = mSuggestions.indexOf(suggestion);
            if (index == -1) continue;

            suggestion.setThumbnailBitmap(mSuggestions.get(index).getThumbnailBitmap());
        }
    }
}
