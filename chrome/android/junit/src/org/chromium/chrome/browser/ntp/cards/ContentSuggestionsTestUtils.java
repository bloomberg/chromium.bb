// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;

import java.util.ArrayList;
import java.util.List;

/** Utilities to make testing content suggestions code easier. */
public final class ContentSuggestionsTestUtils {
    private ContentSuggestionsTestUtils() {}

    public static List<SnippetArticle> createDummySuggestions(int count) {
        List<SnippetArticle> suggestions = new ArrayList<>();
        for (int index = 0; index < count; index++) {
            suggestions.add(new SnippetArticle(KnownCategories.BOOKMARKS,
                    "https://site.com/url" + index, "title" + index, "pub" + index, "txt" + index,
                    "https://site.com/url" + index, "https://amp.site.com/url" + index, 0, 0, 0));
        }
        return suggestions;
    }

    @Deprecated
    public static SuggestionsCategoryInfo createInfo(
            @CategoryInt int category, boolean moreButton, boolean showIfEmpty) {
        return createInfo(category, false, true, moreButton, showIfEmpty);
    }

    public static SuggestionsCategoryInfo createInfo(@CategoryInt int category, boolean moreAction,
            boolean reloadAction, boolean viewAllAction, boolean showIfEmpty) {
        return new SuggestionsCategoryInfo(category, "", ContentSuggestionsCardLayout.FULL_CARD,
                moreAction, reloadAction, viewAllAction, showIfEmpty, "");
    }

    public static SuggestionsSection createSection(boolean hasReloadAction, NodeParent parent,
            NewTabPageManager manager, OfflinePageBridge bridge) {
        return new SuggestionsSection(parent,
                createInfo(42, /*hasMoreAction=*/false, /*hasReloadAction=*/hasReloadAction,
                        /*hasViewAllAction=*/false, /*showIfEmpty=*/true),
                manager, bridge);
    }
}
