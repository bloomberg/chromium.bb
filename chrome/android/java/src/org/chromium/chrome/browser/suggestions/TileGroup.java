// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.support.annotation.Nullable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnCreateContextMenuListener;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.ntp.MostVisitedTileType;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * The model and controller for a group of site suggestion tiles.
 */
public class TileGroup implements MostVisitedSites.Observer {
    /**
     * Performs work in other parts of the system that the {@link TileGroup} should not know about.
     */
    public interface Delegate {
        void removeMostVisitedItem(Tile tile);

        void openMostVisitedItem(int windowDisposition, Tile tile);

        /**
         * Gets the list of most visited sites.
         * @param observer The observer to be notified with the list of sites.
         * @param maxResults The maximum number of sites to retrieve.
         */
        void setMostVisitedSitesObserver(MostVisitedSites.Observer observer, int maxResults);

        /**
         * Called when the NTP has completely finished loading (all views will be inflated
         * and any dependent resources will have been loaded).
         * @param tiles The tiles owned by the {@link TileGroup}. Used to record metrics.
         */
        void onLoadingComplete(Tile[] tiles);

        /**
         * To be called before this instance is abandoned to the garbage collector so it can do any
         * necessary cleanups. This instance must not be used after this method is called.
         */
        void destroy();
    }

    /**
     * An observer for events in the {@link TileGroup}.
     */
    public interface Observer {
        /**
         * Called when any of the tile data has changed, such as an icon, url, or title.
         */
        void onTileDataChanged();

        /**
         * Called when the number of tiles has changed.
         */
        void onTileCountChanged();

        /**
         * Called when a tile icon has changed.
         * @param tile The tile for which the icon has changed.
         */
        void onTileIconChanged(Tile tile);

        /**
         * Called when an asynchronous loading task has started.
         */
        void onLoadTaskAdded();

        /**
         * Called when an asynchronous loading task has completed.
         */
        void onLoadTaskCompleted();
    }

    private static final String TAG = "TileGroup";

    private static final int ICON_CORNER_RADIUS_DP = 4;
    private static final int ICON_TEXT_SIZE_DP = 20;
    private static final int ICON_MIN_SIZE_PX = 48;

    private final Context mContext;
    private final SuggestionsUiDelegate mUiDelegate;
    private final ContextMenuManager mContextMenuManager;
    private final Delegate mTileGroupDelegate;
    private final Observer mObserver;
    private final RoundedIconGenerator mIconGenerator;

    private Tile[] mTiles;
    private int mMinIconSize;
    private int mDesiredIconSize;
    boolean mHasReceivedData;

    public TileGroup(SuggestionsUiDelegate uiDelegate, ContextMenuManager contextMenuManager,
            Delegate tileGroupDelegate, Observer observer) {
        mContext = ContextUtils.getApplicationContext();
        mUiDelegate = uiDelegate;
        mContextMenuManager = contextMenuManager;
        mTileGroupDelegate = tileGroupDelegate;
        mObserver = observer;

        Resources resources = mContext.getResources();
        mDesiredIconSize = resources.getDimensionPixelSize(R.dimen.tile_view_icon_size);
        // On ldpi devices, mDesiredIconSize could be even smaller than ICON_MIN_SIZE_PX.
        mMinIconSize = Math.min(mDesiredIconSize, ICON_MIN_SIZE_PX);
        int desiredIconSizeDp =
                Math.round(mDesiredIconSize / resources.getDisplayMetrics().density);
        int iconColor =
                ApiCompatibilityUtils.getColor(resources, R.color.default_favicon_background_color);
        mIconGenerator = new RoundedIconGenerator(mContext, desiredIconSizeDp, desiredIconSizeDp,
                ICON_CORNER_RADIUS_DP, iconColor, ICON_TEXT_SIZE_DP);
    }

    @Override
    public void onMostVisitedURLsAvailable(final String[] titles, final String[] urls,
            final String[] whitelistIconPaths, final int[] sources) {
        // If no tiles have been built yet, this is the initial load. Build the tiles immediately so
        // the layout is stable during initial rendering. They can be
        // replaced later if there are offline urls, but that will not affect the layout widths and
        // heights. A stable layout enables reliable scroll position initialization.
        if (!mHasReceivedData) {
            buildTiles(titles, urls, whitelistIconPaths, null, sources);
        }

        // TODO(https://crbug.com/607573): We should show offline-available content in a nonblocking
        // way so that responsiveness of the NTP does not depend on ready availability of offline
        // pages.
        Set<String> urlSet = new HashSet<>(Arrays.asList(urls));
        mUiDelegate.getUrlsAvailableOffline(urlSet, new Callback<Set<String>>() {
            @Override
            public void onResult(Set<String> offlineUrls) {
                buildTiles(titles, urls, whitelistIconPaths, offlineUrls, sources);
            }
        });
    }

    @Override
    public void onIconMadeAvailable(String siteUrl) {
        // Get a large icon for the matching tile.
        for (Tile tile : mTiles) {
            if (tile.getUrl().equals(siteUrl)) {
                LargeIconCallback iconCallback =
                        new LargeIconCallbackImpl(tile, /* trackLoadTask = */ false);
                mUiDelegate.getLargeIconForUrl(siteUrl, mMinIconSize, iconCallback);
                break;
            }
        }
    }

    /**
     * Instructs this instance to start listening for data. The {@link TileGroup.Observer} may be
     * called immediately if new data is received synchronously.
     * @param maxResults The maximum number of sites to retrieve.
     */
    public void startObserving(int maxResults) {
        mTileGroupDelegate.setMostVisitedSitesObserver(this, maxResults);
    }

    /**
     * Renders tile views in the given {@link TileGridLayout}, reusing existing tile views where
     * possible because view inflation and icon loading are slow.
     * @param tileGridLayout The layout to render the tile views into.
     * @param trackLoadTasks Whether to track load tasks.
     */
    public void renderTileViews(TileGridLayout tileGridLayout, boolean trackLoadTasks) {
        // Map the old tile views by url so they can be reused later.
        Map<String, TileView> oldTileViews = new HashMap<>();
        int childCount = tileGridLayout.getChildCount();
        for (int i = 0; i < childCount; i++) {
            TileView tileView = (TileView) tileGridLayout.getChildAt(i);
            oldTileViews.put(tileView.getTile().getUrl(), tileView);
        }

        // Remove all views from the layout because even if they are reused later they'll have to be
        // added back in the correct order.
        tileGridLayout.removeAllViews();

        for (final Tile tile : mTiles) {
            // First see if an old view can be reused.
            if (oldTileViews.containsKey(tile.getUrl())) {
                TileView oldTileView = oldTileViews.get(tile.getUrl());
                if (TextUtils.equals(tile.getTitle(), oldTileView.getTile().getTitle())
                        && tile.isOfflineAvailable() == oldTileView.getTile().isOfflineAvailable()
                        && TextUtils.equals(tile.getWhitelistIconPath(),
                                   oldTileView.getTile().getWhitelistIconPath())) {
                    // Prevent further reuse. https://crbug.com/690926
                    assert oldTileView.getParent() == null;
                    oldTileViews.remove(tile.getUrl());

                    tileGridLayout.addView(oldTileView);

                    // Re-render the icon because it may not have been painted when re-added.
                    oldTileView.renderIcon();
                    continue;
                }
            }

            // No view was reused, create a new one.
            TileView tileView = buildTileView(tile, tileGridLayout, trackLoadTasks);

            tileView.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View view) {
                    mTileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.CURRENT_TAB, tile);
                }
            });

            tileView.setOnCreateContextMenuListener(new OnCreateContextMenuListener() {
                @Override
                public void onCreateContextMenu(
                        ContextMenu menu, View view, ContextMenuInfo menuInfo) {
                    mContextMenuManager.createContextMenu(
                            menu, view, new ContextMenuManager.Delegate() {
                                @Override
                                public void openItem(int windowDisposition) {
                                    mTileGroupDelegate.openMostVisitedItem(windowDisposition, tile);
                                }

                                @Override
                                public void removeItem() {
                                    mTileGroupDelegate.removeMostVisitedItem(tile);
                                }

                                @Override
                                public String getUrl() {
                                    return tile.getUrl();
                                }

                                @Override
                                public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
                                    return true;
                                }

                                @Override
                                public void onContextMenuCreated() {}
                            });
                }
            });
            tileGridLayout.addView(tileView);
        }
    }

    public Tile[] getTiles() {
        return Arrays.copyOf(mTiles, mTiles.length);
    }

    public boolean hasReceivedData() {
        return mHasReceivedData;
    }

    private void buildTiles(String[] titles, String[] urls, String[] whitelistIconPaths,
            @Nullable Set<String> offlineUrls, int[] sources) {
        int oldTileCount = mTiles == null ? 0 : mTiles.length;
        mTiles = new Tile[titles.length];

        boolean isInitialLoad = !mHasReceivedData;
        mHasReceivedData = true;

        for (int i = 0; i < titles.length; i++) {
            boolean offlineAvailable = offlineUrls != null && offlineUrls.contains(urls[i]);
            mTiles[i] = new Tile(
                    titles[i], urls[i], whitelistIconPaths[i], offlineAvailable, i, sources[i]);
        }

        if (oldTileCount != mTiles.length) mObserver.onTileCountChanged();
        if (isInitialLoad) mObserver.onLoadTaskCompleted();
        mObserver.onTileDataChanged();
    }

    /**
     * Inflates a new tile view, initializes it, and loads an icon for it.
     * @param tile The tile that holds the data to populate the new tile view.
     * @param parentView The parent of the new tile view.
     * @param trackLoadTask Whether to track a load task.
     * @return The new tile view.
     */
    private TileView buildTileView(Tile tile, ViewGroup parentView, boolean trackLoadTask) {
        TileView tileView = (TileView) LayoutInflater.from(parentView.getContext())
                                    .inflate(R.layout.tile_view, parentView, false);
        tileView.initialize(tile);

        LargeIconCallback iconCallback = new LargeIconCallbackImpl(tile, trackLoadTask);
        if (trackLoadTask) mObserver.onLoadTaskAdded();
        if (!loadWhitelistIcon(tile, iconCallback)) {
            mUiDelegate.getLargeIconForUrl(tile.getUrl(), mMinIconSize, iconCallback);
        }

        return tileView;
    }

    private boolean loadWhitelistIcon(Tile tile, LargeIconCallback iconCallback) {
        if (tile.getWhitelistIconPath().isEmpty()) return false;

        // TODO(mvanouwerkerk): Consider using an AsyncTask to reduce rendering jank.
        Bitmap bitmap = BitmapFactory.decodeFile(tile.getWhitelistIconPath());
        if (bitmap == null) {
            Log.d(TAG, "Image decoding failed: %s", tile.getWhitelistIconPath());
            return false;
        }
        iconCallback.onLargeIconAvailable(bitmap, Color.BLACK, false);
        return true;
    }

    private class LargeIconCallbackImpl implements LargeIconCallback {
        private final Tile mTile;
        private final boolean mTrackLoadTask;

        private LargeIconCallbackImpl(Tile tile, boolean trackLoadTask) {
            mTile = tile;
            mTrackLoadTask = trackLoadTask;
        }

        @Override
        public void onLargeIconAvailable(
                @Nullable Bitmap icon, int fallbackColor, boolean isFallbackColorDefault) {
            if (icon == null) {
                mIconGenerator.setBackgroundColor(fallbackColor);
                icon = mIconGenerator.generateIconForUrl(mTile.getUrl());
                mTile.setIcon(new BitmapDrawable(mContext.getResources(), icon));
                mTile.setType(isFallbackColorDefault ? MostVisitedTileType.ICON_DEFAULT
                                                     : MostVisitedTileType.ICON_COLOR);
            } else {
                RoundedBitmapDrawable roundedIcon =
                        RoundedBitmapDrawableFactory.create(mContext.getResources(), icon);
                int cornerRadius = Math.round(ICON_CORNER_RADIUS_DP
                        * mContext.getResources().getDisplayMetrics().density * icon.getWidth()
                        / mDesiredIconSize);
                roundedIcon.setCornerRadius(cornerRadius);
                roundedIcon.setAntiAlias(true);
                roundedIcon.setFilterBitmap(true);
                mTile.setIcon(roundedIcon);
                mTile.setType(MostVisitedTileType.ICON_REAL);
            }
            mObserver.onTileIconChanged(mTile);
            if (mTrackLoadTask) mObserver.onLoadTaskCompleted();
        }
    }
}
