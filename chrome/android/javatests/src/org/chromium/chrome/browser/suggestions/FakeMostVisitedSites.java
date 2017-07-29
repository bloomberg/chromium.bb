// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A fake implementation of MostVisitedSites that returns a fixed list of most visited sites.
 */
public class FakeMostVisitedSites implements MostVisitedSites {
    private final List<String> mBlacklistedUrls = new ArrayList<>();

    private String[] mTitles = new String[] {};
    private String[] mUrls = new String[] {};
    private String[] mWhitelistIconPaths = new String[] {};
    private int[] mSources = new int[] {};
    private Observer mObserver;

    @Override
    public void destroy() {}

    @Override
    public void setObserver(Observer observer, int numResults) {
        mObserver = observer;
        notifyTileSuggestionsAvailable();
    }

    @Override
    public void addBlacklistedUrl(String url) {
        mBlacklistedUrls.add(url);
    }

    @Override
    public void removeBlacklistedUrl(String url) {
        mBlacklistedUrls.remove(url);
    }

    /**
     * @return Whether blacklistUrl() has been called on the given url.
     */
    public boolean isUrlBlacklisted(String url) {
        return mBlacklistedUrls.contains(url);
    }

    @Override
    public void recordPageImpression(int tilesCount) {
        // Metrics are stubbed out.
    }

    @Override
    public void recordTileImpression(int index, int type, int source, String url) {
        // Metrics are stubbed out.
    }

    @Override
    public void recordOpenedMostVisitedItem(int index, int tileType, int source) {
        //  Metrics are stubbed out.
    }

    /**
     * Sets new tile suggestion data. If there is an observer it is notified.
     * @param titles The titles of the site suggestions.
     * @param urls The URLs of the site suggestions.
     * @param whitelistIconPaths The paths to the icon image files for whitelisted tiles, empty
     *                           strings otherwise.
     * @param sources For each tile, the {@code NTPTileSource} that generated the tile.
     */
    public void setTileSuggestions(
            String[] titles, String[] urls, String[] whitelistIconPaths, int[] sources) {
        assert titles.length == urls.length;
        mTitles = titles.clone();
        mUrls = urls.clone();
        mWhitelistIconPaths = whitelistIconPaths.clone();
        mSources = sources.clone();
        notifyTileSuggestionsAvailable();
    }

    /**
     * Sets new tile suggestion data, generating dummy data for the missing properties.
     * If there is an observer it is notified.
     *
     * @param urls The URLs of the site suggestions.
     * @see #setTileSuggestions(String[], String[], String[], int[])
     */
    public void setTileSuggestions(String... urls) {
        String[] whitelistIconPaths = new String[urls.length];
        Arrays.fill(whitelistIconPaths, "");

        int[] sources = new int[urls.length];
        Arrays.fill(sources, TileSource.TOP_SITES);

        setTileSuggestions(urls, urls.clone(), whitelistIconPaths, sources);
    }

    private void notifyTileSuggestionsAvailable() {
        if (mObserver == null) return;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mObserver.onSiteSuggestionsAvailable(
                        MostVisitedSitesBridge.buildSiteSuggestions(mTitles.clone(), mUrls.clone(),
                                mWhitelistIconPaths.clone(), mSources.clone()));
            }
        });
    }
}
