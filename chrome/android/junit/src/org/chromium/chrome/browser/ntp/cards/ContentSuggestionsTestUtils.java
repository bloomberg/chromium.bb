// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout.ContentSuggestionsCardLayoutEnum;
import org.chromium.chrome.browser.ntp.snippets.FakeSuggestionsSource;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

import java.util.ArrayList;
import java.util.List;

/** Utilities to make testing content suggestions code easier. */
public final class ContentSuggestionsTestUtils {
    private ContentSuggestionsTestUtils() {}

    public static List<SnippetArticle> createDummySuggestions(
            int count, @CategoryInt int category) {
        List<SnippetArticle> suggestions = new ArrayList<>();
        for (int index = 0; index < count; index++) {
            suggestions.add(
                    new SnippetArticle(category, "https://site.com/url" + index, "title" + index,
                            "pub" + index, "txt" + index, "https://site.com/url" + index, 0, 0));
        }
        return suggestions;
    }

    /**
     * @deprecated The hardcoded category is a common source of bugs. Prefer
     * {@link #createDummySuggestions(int, int)}
     */
    @Deprecated
    public static List<SnippetArticle> createDummySuggestions(int count) {
        return createDummySuggestions(count, KnownCategories.BOOKMARKS);
    }

    /**
     * Registers a category according to the provided category info.
     * @return the suggestions added to the newly registered category.
     */
    public static List<SnippetArticle> registerCategory(FakeSuggestionsSource suggestionsSource,
            @CategoryInt int category, int suggestionCount) {
        // Important: showIfEmpty flag to true.
        SuggestionsCategoryInfo categoryInfo =
                new CategoryInfoBuilder(category).withReloadAction().showIfEmpty().build();
        return registerCategory(suggestionsSource, categoryInfo, suggestionCount);
    }

    /**
     * Registers a category that has a reload action and is shown if empty.
     * @return the suggestions added to the newly registered category.
     */
    public static List<SnippetArticle> registerCategory(FakeSuggestionsSource suggestionsSource,
            SuggestionsCategoryInfo categoryInfo, int suggestionCount) {
        // FakeSuggestionSource does not provide suggestions if the category's status is not
        // AVAILABLE.
        suggestionsSource.setStatusForCategory(
                categoryInfo.getCategory(), CategoryStatus.AVAILABLE);
        suggestionsSource.setInfoForCategory(categoryInfo.getCategory(), categoryInfo);

        List<SnippetArticle> suggestions =
                createDummySuggestions(suggestionCount, categoryInfo.getCategory());
        suggestionsSource.setSuggestionsForCategory(categoryInfo.getCategory(), suggestions);
        return suggestions;
    }

    public static String viewTypeToString(@ItemViewType int viewType) {
        switch (viewType) {
            case ItemViewType.ABOVE_THE_FOLD:
                return "ABOVE_THE_FOLD";
            case ItemViewType.HEADER:
                return "HEADER";
            case ItemViewType.SNIPPET:
                return "SNIPPET";
            case ItemViewType.SPACING:
                return "SPACING";
            case ItemViewType.STATUS:
                return "STATUS";
            case ItemViewType.PROGRESS:
                return "PROGRESS";
            case ItemViewType.ACTION:
                return "ACTION";
            case ItemViewType.FOOTER:
                return "FOOTER";
            case ItemViewType.PROMO:
                return "PROMO";
            case ItemViewType.ALL_DISMISSED:
                return "ALL_DISMISSED";
        }
        throw new AssertionError();
    }

    /**
     * Uses the builder pattern to simplify constructing category info objects for tests.
     */
    public static class CategoryInfoBuilder {
        @CategoryInt
        private final int mCategory;
        private boolean mHasMoreAction;
        private boolean mHasViewAllAction;
        private boolean mHasReloadAction;
        private boolean mShowIfEmpty;
        private String mTitle = "";
        private String mNoSuggestionsMessage = "";
        @ContentSuggestionsCardLayoutEnum
        private int mCardLayout = ContentSuggestionsCardLayout.FULL_CARD;

        public CategoryInfoBuilder(@CategoryInt int category) {
            mCategory = category;
        }

        public CategoryInfoBuilder withMoreAction() {
            mHasMoreAction = true;
            return this;
        }

        public CategoryInfoBuilder withViewAllAction() {
            mHasViewAllAction = true;
            return this;
        }

        public CategoryInfoBuilder withReloadAction() {
            mHasReloadAction = true;
            return this;
        }

        public CategoryInfoBuilder showIfEmpty() {
            mShowIfEmpty = true;
            return this;
        }

        public CategoryInfoBuilder withTitle(String title) {
            mTitle = title;
            return this;
        }

        public CategoryInfoBuilder withNoSuggestionsMessage(String message) {
            mNoSuggestionsMessage = message;
            return this;
        }

        public CategoryInfoBuilder withCardLayout(
                @ContentSuggestionsCardLayoutEnum int cardLayout) {
            mCardLayout = cardLayout;
            return this;
        }

        public SuggestionsCategoryInfo build() {
            return new SuggestionsCategoryInfo(mCategory, mTitle, mCardLayout, mHasMoreAction,
                    mHasReloadAction, mHasViewAllAction, mShowIfEmpty, mNoSuggestionsMessage);
        }
    }
}
