// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.graphics.Bitmap;
import android.support.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.MostVisitedSites;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Handles SiteSuggestion loading from MostVisitedSites.
 */
class SiteSuggestionsMediator implements MostVisitedSites.Observer {
    private static final int NUM_FETCHED_SITES = 8;
    // The ML tiles could show anywhere between 2-5 tiles. To ensure that the initial index
    // would land on % n == 0 and be roughly Integer.MAX / 2, where n could be [2,5],
    // we divide by 240 (which is 5!) before multiplying by 120.
    // TODO(chili): Maybe better formula for if this needs to be more generic.
    private static final int INITIAL_SCROLLED_POSITION = Integer.MAX_VALUE / 240 * 120;
    private static final int MAX_DISPLAYED_TILES = 4;

    private PropertyModel mModel;
    private ImageFetcher mImageFetcher;
    private MostVisitedSites mMostVisitedSites;
    private int mIconSize;

    SiteSuggestionsMediator(
            PropertyModel model, Profile profile, ImageFetcher imageFetcher, int minIconSize) {
        mModel = model;
        mImageFetcher = imageFetcher;
        mIconSize = minIconSize;
        mMostVisitedSites =
                SuggestionsDependencyFactory.getInstance().createMostVisitedSites(profile);
        mMostVisitedSites.setObserver(this, NUM_FETCHED_SITES);
    }

    @Override
    public void onSiteSuggestionsAvailable(List<SiteSuggestion> siteSuggestions) {
        for (SiteSuggestion suggestion : siteSuggestions) {
            mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY)
                    .add(SiteSuggestionModel.getSiteSuggestionModel(suggestion));
        }

        if (siteSuggestions.size() > 0) {
            mModel.set(SiteSuggestionsCoordinator.CURRENT_INDEX_KEY, INITIAL_SCROLLED_POSITION);
        }
        // ItemCount is max number of tiles to be displayed.
        int itemCount = siteSuggestions.size() > MAX_DISPLAYED_TILES ? MAX_DISPLAYED_TILES
                                                                     : siteSuggestions.size();
        // Total item count is 1 more to account for "all apps".
        mModel.set(SiteSuggestionsCoordinator.ITEM_COUNT_KEY, itemCount + 1);
    }

    @Override
    public void onIconMadeAvailable(String siteUrl) {
        for (PropertyModel suggestion : mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY)) {
            if (suggestion.get(SiteSuggestionModel.URL_KEY).equals(siteUrl)) {
                mImageFetcher.makeLargeIconRequest(siteUrl, mIconSize,
                        (@Nullable Bitmap icon, int fallbackColor, boolean isFallbackDefault,
                                int iconType) -> {
                            if (icon != null) {
                                suggestion.set(SiteSuggestionModel.ICON_KEY, icon);
                            }
                        });
            }
        }
    }

    public void destroy() {
        mMostVisitedSites.destroy();
    }
}
