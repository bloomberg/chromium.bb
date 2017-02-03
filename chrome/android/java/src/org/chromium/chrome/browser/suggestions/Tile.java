// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;

import org.chromium.chrome.browser.ntp.MostVisitedTileType;

/**
 * Holds the details of a site tile.
 */
public class Tile {
    private final String mTitle;
    private final String mUrl;
    private final String mWhitelistIconPath;
    private final boolean mOfflineAvailable;
    private int mIndex;
    private int mTileType = MostVisitedTileType.NONE;
    private final int mSource;

    @Nullable
    private Drawable mIcon;

    /**
     * @param title The title of the page.
     * @param url The URL of the page.
     * @param whitelistIconPath The path to the icon image file, if this is a whitelisted tile.
     * Empty otherwise.
     * @param offlineAvailable Whether there is an offline copy of the URL available.
     * @param index The index of this tile in the list of tiles.
     * @param source The {@code MostVisitedSource} that generated this tile.
     */
    public Tile(String title, String url, String whitelistIconPath, boolean offlineAvailable,
            int index, int source) {
        mTitle = title;
        mUrl = url;
        mWhitelistIconPath = whitelistIconPath;
        mOfflineAvailable = offlineAvailable;
        mIndex = index;
        mSource = source;
    }

    /**
     * @return The URL of this tile.
     */
    public String getUrl() {
        return mUrl;
    }

    /**
     * @return The title of this tile.
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * @return The path of the whitelist icon associated with the URL.
     */
    public String getWhitelistIconPath() {
        return mWhitelistIconPath;
    }

    /**
     * @return Whether this tile is available offline.
     */
    public boolean isOfflineAvailable() {
        return mOfflineAvailable;
    }

    /**
     * @return The index of this tile in the list of tiles.
     */
    public int getIndex() {
        return mIndex;
    }

    /**
     * Updates this tile's index in the list of tiles.
     */
    public void setIndex(int index) {
        mIndex = index;
    }

    /**
     * @return The visual type of this tile. Valid values are listed in
     *         {@link MostVisitedTileType}.
     */
    public int getTileType() {
        return mTileType;
    }

    /**
     * Sets the visual type of this tile. Valid values are listed in
     * {@link MostVisitedTileType}.
     */
    public void setTileType(int type) {
        mTileType = type;
    }

    /**
     * @return The source of this tile.  Used for metrics tracking. Valid values are listed in
     * {@code MostVisitedSource}.
     */
    public int getSource() {
        return mSource;
    }

    /**
     * @return The icon, may be null.
     */
    @Nullable
    public Drawable getIcon() {
        return mIcon;
    }

    /**
     * Updates the icon drawable.
     */
    public void setIcon(@Nullable Drawable icon) {
        mIcon = icon;
    }
}
