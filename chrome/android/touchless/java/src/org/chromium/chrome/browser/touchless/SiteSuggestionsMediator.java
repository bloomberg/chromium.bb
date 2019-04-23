// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.explore_sites.ExploreSitesBridge;
import org.chromium.chrome.browser.explore_sites.ExploreSitesCategory;
import org.chromium.chrome.browser.explore_sites.ExploreSitesEnums;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.MostVisitedSites;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;
import org.chromium.ui.widget.Toast;

import java.util.HashSet;
import java.util.List;

/**
 * Handles SiteSuggestion loading from MostVisitedSites. This includes images and reloading more
 * suggestions when needed.
 */

class SiteSuggestionsMediator implements MostVisitedSites.Observer,
                                         PropertyObservable.PropertyObserver<PropertyKey>,
                                         FocusableComponent {
    private static final int NUM_FETCHED_SITES = 8;
    // The ML tiles could show anywhere between 2-5 tiles. To ensure that the initial index
    // would land on % n == 0 and be roughly Integer.MAX / 2, where n could be [2,5],
    // we divide by 240 (which is 5!) before multiplying by 120.
    // TODO(chili): Maybe better formula for if this needs to be more generic.
    private static final int INITIAL_SCROLLED_POSITION = Integer.MAX_VALUE / 240 * 120;
    private static final int MAX_DISPLAYED_TILES = 5;

    private PropertyModel mModel;
    private ImageFetcher mImageFetcher;
    private MostVisitedSites mMostVisitedSites;
    private Profile mProfile;
    private int mIconSize;

    SiteSuggestionsMediator(
            PropertyModel model, Profile profile, ImageFetcher imageFetcher, int minIconSize) {
        mModel = model;
        mImageFetcher = imageFetcher;
        mIconSize = minIconSize;
        mProfile = profile;
        mMostVisitedSites =
                SuggestionsDependencyFactory.getInstance().createMostVisitedSites(mProfile);
        mMostVisitedSites.setObserver(this, NUM_FETCHED_SITES);
        mModel.addObserver(this);
        ExploreSitesBridge.getEspCatalog(mProfile, this::onGetEspCatalog);
    }

    @Override
    public void onSiteSuggestionsAvailable(List<SiteSuggestion> siteSuggestions) {
        HashSet<String> urls = new HashSet<>();
        for (PropertyModel model : mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY)) {
            urls.add(model.get(SiteSuggestionModel.URL_KEY));
        }

        // Map each SiteSuggestion into a PropertyModel representation and fetch the icon.
        for (SiteSuggestion suggestion : siteSuggestions) {
            // Do not put duplicates.
            if (urls.contains(suggestion.url)) continue;

            PropertyModel siteSuggestion = SiteSuggestionModel.getSiteSuggestionModel(suggestion);
            mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY).add(siteSuggestion);
            if (suggestion.whitelistIconPath.isEmpty()) {
                makeIconRequest(siteSuggestion);
            } else {
                AsyncTask<Bitmap> task = new AsyncTask<Bitmap>() {
                    @Override
                    protected Bitmap doInBackground() {
                        return BitmapFactory.decodeFile(suggestion.whitelistIconPath);
                    }
                    @Override
                    protected void onPostExecute(Bitmap icon) {
                        if (icon == null) makeIconRequest(siteSuggestion);
                    }
                };
                task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            }
        }
        // Total item count is 1 more than number of site suggestions to account for "all apps".
        mModel.set(SiteSuggestionsCoordinator.ITEM_COUNT_KEY, getItemCount());

        // If we fetched site suggestions the first time, set initial scrolled position.
        // We don't want to set scrolled position if we've already set position before.
        if (siteSuggestions.size() > 0
                && mModel.get(SiteSuggestionsCoordinator.CURRENT_INDEX_KEY) == 0) {
            mModel.set(SiteSuggestionsCoordinator.CURRENT_INDEX_KEY, INITIAL_SCROLLED_POSITION);
        }
    }

    @Override
    public void onIconMadeAvailable(String siteUrl) {
        // Update icon if an icon was made available.
        for (PropertyModel suggestion : mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY)) {
            if (suggestion.get(SiteSuggestionModel.URL_KEY).equals(siteUrl)) {
                makeIconRequest(suggestion);
            }
        }
    }

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
        if (propertyKey == SiteSuggestionsCoordinator.REMOVAL_KEY) {
            String removalUrl = mModel.get(SiteSuggestionsCoordinator.REMOVAL_KEY);
            mMostVisitedSites.addBlacklistedUrl(removalUrl);

            Toast.makeText(ContextUtils.getApplicationContext(), R.string.most_visited_item_removed,
                         Toast.LENGTH_SHORT)
                    .show();

            // When we remove an item, reset the item count key.
            int itemCount = getItemCount();
            // Total item count is 1 more to account for "all apps".
            mModel.set(SiteSuggestionsCoordinator.ITEM_COUNT_KEY, itemCount);
            // When removal of a site causes us to have fewer sites than we want to display, fetch
            // again.
            if (itemCount < MAX_DISPLAYED_TILES) {
                mMostVisitedSites.setObserver(this, NUM_FETCHED_SITES);
            }
        }
    }

    public void destroy() {
        mMostVisitedSites.destroy();
    }

    @Override
    public void requestFocus() {
        mModel.set(SiteSuggestionsCoordinator.SHOULD_FOCUS_VIEW, true);
    }

    @Override
    public void setOnFocusListener(Runnable listener) {
        mModel.set(SiteSuggestionsCoordinator.ON_FOCUS_CALLBACK, listener);
    }

    private int getItemCount() {
        // Item count is number of site suggestions available, up to MAX_DISPLAYED_TILES, + 1 for
        // "All apps".
        return (mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY).size() > MAX_DISPLAYED_TILES
                               ? MAX_DISPLAYED_TILES
                               : mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY).size())
                + 1;
    }

    private void makeIconRequest(PropertyModel suggestion) {
        // Fetches the icon for a site suggestion.
        mImageFetcher.makeLargeIconRequest(suggestion.get(SiteSuggestionModel.URL_KEY), mIconSize,
                (@Nullable Bitmap icon, int fallbackColor, boolean isFallbackDefault,
                        int iconType) -> {
                    if (icon != null) {
                        suggestion.set(SiteSuggestionModel.ICON_KEY, icon);
                    }
                });
    }

    private void onGetEspCatalog(List<ExploreSitesCategory> categoryList) {
        boolean loadCatalogFromNetwork = categoryList == null || categoryList.isEmpty();
        if (loadCatalogFromNetwork) {
            ExploreSitesBridge.updateCatalogFromNetwork(mProfile, true, (success) -> {});
            RecordHistogram.recordEnumeratedHistogram("ExploreSites.CatalogUpdateRequestSource",
                    ExploreSitesEnums.CatalogUpdateRequestSource.NEW_TAB_PAGE,
                    ExploreSitesEnums.CatalogUpdateRequestSource.NUM_ENTRIES);
        }
        RecordHistogram.recordBooleanHistogram(
                "ExploreSites.NTPLoadingCatalogFromNetwork", loadCatalogFromNetwork);
    }
}
