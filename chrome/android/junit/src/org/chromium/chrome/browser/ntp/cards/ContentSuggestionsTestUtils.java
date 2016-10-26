// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

import java.util.ArrayList;
import java.util.List;

/** Utilities to make testing content suggestions code easier. */
public final class ContentSuggestionsTestUtils {
    private ContentSuggestionsTestUtils() {}

    public static List<SnippetArticle> createDummySuggestions(int count) {
        List<SnippetArticle> suggestions = new ArrayList<>();
        for (int index = 0; index < count; index++) {
            suggestions.add(new SnippetArticle(0, "https://site.com/url" + index, "title" + index,
                    "pub" + index, "txt" + index, "https://site.com/url" + index,
                    "https://amp.site.com/url" + index, 0, 0, 0,
                    ContentSuggestionsCardLayout.FULL_CARD));
        }
        return suggestions;
    }

    public static SuggestionsCategoryInfo createInfo(
            @CategoryInt int category, boolean moreButton, boolean showIfEmpty) {
        return new SuggestionsCategoryInfo(
                category, "", ContentSuggestionsCardLayout.FULL_CARD, moreButton, showIfEmpty, "");
    }

    public static SuggestionsSection createSection(
            boolean moreButton, boolean showIfEmpty, NodeParent parent) {
        SuggestionsCategoryInfo info = createInfo(42, moreButton, showIfEmpty);
        return new SuggestionsSection(parent, info);
    }
}
