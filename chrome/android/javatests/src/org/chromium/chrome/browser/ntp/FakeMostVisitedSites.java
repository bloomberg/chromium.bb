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

    private final String[] mMostVisitedTitles;
    private final String[] mMostVisitedUrls;
    private final String[] mMostVisitedWhitelistIconPaths;
    private final int[] mMostVisitedSources;

    private final List<String> mBlacklistedUrls = new ArrayList<String>();

    /**
     * @param p The profile to associated most visited with.
     * @param mostVisitedTitles The titles of the fixed list of most visited sites.
     * @param mostVisitedUrls The URLs of the fixed list of most visited sites.
     */
    public FakeMostVisitedSites(Profile p, String[] mostVisitedTitles, String[] mostVisitedUrls,
            String[] mostVisitedWhitelistIconPaths, int[] mostVisitedSources) {
        super(p);
        assert mostVisitedTitles.length == mostVisitedUrls.length;
        mMostVisitedTitles = mostVisitedTitles.clone();
        mMostVisitedUrls = mostVisitedUrls.clone();
        mMostVisitedWhitelistIconPaths = mostVisitedWhitelistIconPaths.clone();
        mMostVisitedSources = mostVisitedSources.clone();
    }

    @Override
    public void setMostVisitedURLsObserver(final MostVisitedURLsObserver observer, int numResults) {
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                observer.onMostVisitedURLsAvailable(mMostVisitedTitles.clone(),
                        mMostVisitedUrls.clone(), mMostVisitedWhitelistIconPaths.clone(),
                        mMostVisitedSources.clone());
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
    public void recordPageImpression(int[] sources, int[] tileTypes) {
        // Metrics are stubbed out.
    }

    @Override
    public void recordOpenedMostVisitedItem(int index, int tileType, int source) {
        //  Metrics are stubbed out.
    }
}
