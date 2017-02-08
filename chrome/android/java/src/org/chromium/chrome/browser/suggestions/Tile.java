// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;

import org.chromium.chrome.browser.ntp.MostVisitedTileType;
import org.chromium.chrome.browser.ntp.MostVisitedTileType.MostVisitedTileTypeEnum;
import org.chromium.chrome.browser.ntp.NTPTileSource.NTPTileSourceEnum;

/**
 * Holds the details to populate a site suggestion tile.
 */
public class Tile {
    private final String mTitle;
    private final String mUrl;
    private final String mWhitelistIconPath;
    private final boolean mOfflineAvailable;
    private final int mIndex;

    @NTPTileSourceEnum
    private final int mSource;

    @MostVisitedTileTypeEnum
    private int mType = MostVisitedTileType.NONE;

    @Nullable
    private Drawable mIcon;

    /**
     * @param title The tile title.
     * @param url The site URL.
     * @param whitelistIconPath The path to the icon image file, if this is a whitelisted tile.
     * Empty otherwise.
     * @param offlineAvailable Whether there is an offline copy of the URL available.
     * @param index The index of this tile in the list of tiles.
     * @param source The {@code NTPTileSource} that generated this tile.
     */
    public Tile(String title, String url, String whitelistIconPath, boolean offlineAvailable,
            int index, @NTPTileSourceEnum int source) {
        mTitle = title;
        mUrl = url;
        mWhitelistIconPath = whitelistIconPath;
        mOfflineAvailable = offlineAvailable;
        mIndex = index;
        mSource = source;
    }

    /**
     * @return The site URL of this tile.
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
     * @return The source of this tile. Used for metrics tracking. Valid values are listed in
     * {@code NTPTileSource}.
     */
    @NTPTileSourceEnum
    public int getSource() {
        return mSource;
    }

    /**
     * @return The visual type of this tile. Valid values are listed in
     *         {@link MostVisitedTileType}.
     */
    @MostVisitedTileTypeEnum
    public int getType() {
        return mType;
    }

    /**
     * Sets the visual type of this tile. Valid values are listed in
     * {@link MostVisitedTileType}.
     */
    public void setType(@MostVisitedTileTypeEnum int type) {
        mType = type;
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
