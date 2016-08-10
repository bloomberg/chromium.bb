// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout.ContentSuggestionsCardLayoutEnum;

/**
 * Contains static meta information about a Category. Equivalent of the CategoryInfo class in
 * components/ntp_snippets/category_info.h.
 */
public class SuggestionsCategoryInfo {
    /**
     * Localized title of the category.
     */
    private final String mTitle;

    /**
     * Layout of the cards to be used to display suggestions in this category.
     */
    @ContentSuggestionsCardLayoutEnum
    private final int mCardLayout;

    public SuggestionsCategoryInfo(String title, @ContentSuggestionsCardLayoutEnum int cardLayout) {
        this.mTitle = title;
        this.mCardLayout = cardLayout;
    }

    public String getTitle() {
        return this.mTitle;
    }

    @ContentSuggestionsCardLayoutEnum
    public int getCardLayout() {
        return this.mCardLayout;
    }
}
