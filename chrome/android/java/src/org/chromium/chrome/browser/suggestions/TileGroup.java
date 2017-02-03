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
import org.chromium.chrome.browser.ntp.MostVisitedItemView;
import org.chromium.chrome.browser.ntp.MostVisitedTileType;
import org.chromium.chrome.browser.ntp.TitleUtil;
import org.chromium.chrome.browser.profiles.MostVisitedSites.MostVisitedURLsObserver;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * The model and controller for a group of site suggestion tiles.
 */
public class TileGroup implements MostVisitedURLsObserver {
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
        void setMostVisitedURLsObserver(MostVisitedURLsObserver observer, int maxResults);

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
         * Called when the first data has been received and processed.
         */
        void onInitialTileDataLoaded();

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
        mDesiredIconSize = resources.getDimensionPixelSize(R.dimen.most_visited_icon_size);
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
        mTileGroupDelegate.setMostVisitedURLsObserver(this, maxResults);
    }

    public void renderTileViews(ViewGroup parentView, boolean trackLoadTasks) {
        parentView.removeAllViews();

        for (final Tile tile : mTiles) {
            View tileView = buildTileView(tile, parentView, trackLoadTasks);

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

            parentView.addView(tileView);
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
        Tile[] oldTiles = mTiles;
        int oldTileCount = oldTiles == null ? 0 : oldTiles.length;
        mTiles = new Tile[titles.length];

        boolean isInitialLoad = !mHasReceivedData;
        mHasReceivedData = true;

        // Add the tile views to the layout.
        for (int i = 0; i < titles.length; i++) {
            String url = urls[i];
            String title = titles[i];
            String whitelistIconPath = whitelistIconPaths[i];
            int source = sources[i];
            boolean offlineAvailable = offlineUrls != null && offlineUrls.contains(url);

            // Look for an existing tile to reuse.
            // TODO(mvanouwerkerk): Ensure we don't create views and load icons more often than
            // necessary.
            Tile tile = null;
            for (int j = 0; j < oldTileCount; j++) {
                Tile oldTile = oldTiles[j];
                if (oldTile != null && TextUtils.equals(url, oldTile.getUrl())
                        && TextUtils.equals(title, oldTile.getTitle())
                        && offlineAvailable == oldTile.isOfflineAvailable()
                        && whitelistIconPath.equals(oldTile.getWhitelistIconPath())) {
                    tile = oldTile;
                    tile.setIndex(i);
                    oldTiles[j] = null;
                    break;
                }
            }

            // If nothing can be reused, create a new tile.
            if (tile == null) {
                tile = new Tile(title, url, whitelistIconPath, offlineAvailable, i, source);
            }
            mTiles[i] = tile;
        }

        if (oldTileCount != mTiles.length) mObserver.onTileCountChanged();
        if (isInitialLoad) {
            mObserver.onLoadTaskCompleted();
            mObserver.onInitialTileDataLoaded();
        }
        mObserver.onTileDataChanged();
    }

    private View buildTileView(Tile tile, ViewGroup parentView, boolean trackLoadTask) {
        MostVisitedItemView view =
                (MostVisitedItemView) LayoutInflater.from(parentView.getContext())
                        .inflate(R.layout.most_visited_item, parentView, false);
        view.setTitle(TitleUtil.getTitleForDisplay(tile.getTitle(), tile.getUrl()));
        view.setOfflineAvailable(tile.isOfflineAvailable());
        view.setIcon(tile.getIcon());
        view.setUrl(tile.getUrl());

        LargeIconCallback iconCallback = new LargeIconCallbackImpl(tile, trackLoadTask);
        if (trackLoadTask) mObserver.onLoadTaskAdded();
        if (!loadWhitelistIcon(tile, iconCallback)) {
            mUiDelegate.getLargeIconForUrl(tile.getUrl(), mMinIconSize, iconCallback);
        }

        return view;
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
                mTile.setTileType(isFallbackColorDefault ? MostVisitedTileType.ICON_DEFAULT
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
                mTile.setTileType(MostVisitedTileType.ICON_REAL);
            }
            mObserver.onTileIconChanged(mTile);
            if (mTrackLoadTask) mObserver.onLoadTaskCompleted();
        }
    }
}
