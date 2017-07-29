// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

/**
 * Data class that holds the site suggestion data provided by the tiles component.
 */
public class SiteSuggestion {
    /** Title of the suggested site. */
    public final String title;

    /** URL of the suggested site. */
    public final String url;

    /** The path to the icon image file for whitelisted tile, empty string otherwise. */
    public final String whitelistIconPath;

    /** the {@code TileSource} that generated the tile. */
    @TileSource
    public final int source;

    public SiteSuggestion(String title, String url, String whitelistIconPath, int source) {
        this.title = title;
        this.url = url;
        this.whitelistIconPath = whitelistIconPath;
        this.source = source;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        SiteSuggestion that = (SiteSuggestion) o;

        if (source != that.source) return false;
        if (!title.equals(that.title)) return false;
        if (!url.equals(that.url)) return false;
        return whitelistIconPath.equals(that.whitelistIconPath);
    }

    @Override
    public int hashCode() {
        int result = title.hashCode();
        result = 31 * result + url.hashCode();
        result = 31 * result + whitelistIconPath.hashCode();
        result = 31 * result + source;
        return result;
    }
}
