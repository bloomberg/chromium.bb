// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.MostVisitedSites;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * A fake implementation of MostVisitedSites that returns a fixed list of most visited sites.
 */
public class FakeMostVisitedSites extends MostVisitedSites {
    private final String[] mTitles;
    private final String[] mUrls;
    private final String[] mWhitelistIconPaths;
    private final int[] mSources;
    private final List<String> mBlacklistedUrls = new ArrayList<>();

    /**
     * @param profile The profile for which to fetch site suggestions.
     * @param titles The titles of the site suggestions.
     * @param urls The URLs of the site suggestions.
     * @param whitelistIconPaths The paths to the icon image files for whitelisted tiles, empty
     *                           strings otherwise.
     * @param sources For each tile, the {@code NTPTileSource} that generated the tile.
     */
    public FakeMostVisitedSites(Profile profile, String[] titles, String[] urls,
            String[] whitelistIconPaths, int[] sources) {
        super(profile);
        assert titles.length == urls.length;
        mTitles = titles.clone();
        mUrls = urls.clone();
        mWhitelistIconPaths = whitelistIconPaths.clone();
        mSources = sources.clone();
    }

    @Override
    public void setMostVisitedURLsObserver(final MostVisitedURLsObserver observer, int numResults) {
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                observer.onMostVisitedURLsAvailable(mTitles.clone(), mUrls.clone(),
                        mWhitelistIconPaths.clone(), mSources.clone());
            }
        });
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
    public void recordPageImpression(int[] sources, int[] tileTypes, String[] tileUrls) {
        // Metrics are stubbed out.
    }

    @Override
    public void recordOpenedMostVisitedItem(int index, int tileType, int source) {
        //  Metrics are stubbed out.
    }
}
