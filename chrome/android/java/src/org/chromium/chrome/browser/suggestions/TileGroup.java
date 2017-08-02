// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Context;
import android.graphics.Bitmap;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.util.SparseArray;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnCreateContextMenuListener;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * The model and controller for a group of site suggestion tiles.
 */
public class TileGroup implements MostVisitedSites.Observer {
    /**
     * Performs work in other parts of the system that the {@link TileGroup} should not know about.
     */
    public interface Delegate {
        /**
         * @param tile The tile corresponding to the most visited item to remove.
         * @param removalUndoneCallback The callback to invoke if the removal is reverted. The
         *                              callback's argument is the URL being restored.
         */
        void removeMostVisitedItem(Tile tile, Callback<String> removalUndoneCallback);

        void openMostVisitedItem(int windowDisposition, Tile tile);

        /**
         * Gets the list of most visited sites.
         * @param observer The observer to be notified with the list of sites.
         * @param maxResults The maximum number of sites to retrieve.
         */
        void setMostVisitedSitesObserver(MostVisitedSites.Observer observer, int maxResults);

        /**
         * Called when the tile group has completely finished loading (all views will be inflated
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
         * Called when the tile group is initialised and when any of the tile data has changed,
         * such as an icon, url, or title.
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
         * Called when the visibility of a tile's offline badge has changed.
         * @param tile The tile for which the visibility of the offline badge has changed.
         */
        void onTileOfflineBadgeVisibilityChanged(Tile tile);
    }

    /**
     * A delegate to allow {@link TileRenderer} to setup behaviours for the newly created views
     * associated to a Tile.
     */
    public interface TileSetupDelegate {
        /**
         * Returns a delegate that will handle user interactions with the view created for the tile.
         */
        TileInteractionDelegate createInteractionDelegate(Tile tile);

        /**
         * Returns a callback to be invoked when the icon for the provided tile is loaded. It will
         * be responsible for updating the tile data and triggering the visual refresh.
         */
        LargeIconBridge.LargeIconCallback createIconLoadCallback(Tile tile);
    }

    /**
     * Constants used to track the current operations on the group and notify the {@link Delegate}
     * when the expected sequence of potentially asynchronous operations is complete.
     */
    @VisibleForTesting
    @IntDef({TileTask.FETCH_DATA, TileTask.SCHEDULE_ICON_FETCH, TileTask.FETCH_ICON})
    @interface TileTask {
        /**
         * An event that should result in new data being loaded happened.
         * Can be an asynchronous task, spanning from when the {@link Observer} is registered to
         * when the initial load completes.
         */
        int FETCH_DATA = 1;

        /**
         * New tile data has been loaded and we are expecting the related icons to be fetched.
         * Can be an asynchronous task, as we rely on it being triggered by the embedder, some time
         * after {@link Observer#onTileDataChanged()} is called.
         */
        int SCHEDULE_ICON_FETCH = 2;

        /**
         * The icon for a tile is being fetched.
         * Asynchronous task, that is started for each icon that needs to be loaded.
         */
        int FETCH_ICON = 3;
    }

    // TODO(dgn) should be generated from C++ enum. Remove when https://crrev.com/c/593651 lands.
    @IntDef({TileSectionType.PERSONALIZED})
    @Retention(RetentionPolicy.SOURCE)
    @interface TileSectionType {
        int PERSONALIZED = 0;
    }

    private final SuggestionsUiDelegate mUiDelegate;
    private final ContextMenuManager mContextMenuManager;
    private final Delegate mTileGroupDelegate;
    private final Observer mObserver;
    private final TileRenderer mTileRenderer;

    /**
     * Tracks the tasks currently in flight.
     *
     * We only care about which ones are pending, not their order, and we can have multiple tasks
     * pending of the same type. Hence exposing the type as Collection rather than List or Set.
     */
    private final Collection<Integer> mPendingTasks = new ArrayList<>();

    /**
     * Access point to offline related features. Will be {@code null} when the badges are disabled.
     * @see ChromeFeatureList#NTP_OFFLINE_PAGES_FEATURE_NAME
     */
    @Nullable
    private final OfflineModelObserver mOfflineModelObserver;

    /**
     * Source of truth for the tile data. Since the objects can change when the data is updated,
     * other objects should not hold references to them but keep track of the URL instead, and use
     * it to retrieve a {@link Tile}.
     * TODO(dgn): If we don't recreate them when loading native data we could keep them around and
     * simplify a few things.
     * @see #findTile(String, int)
     * @see #findTiles(String)
     */
    private SparseArray<List<Tile>> mTileSections = createEmptyTileData();

    /** Most recently received tile data that has not been displayed yet. */
    @Nullable
    private List<Tile> mPendingTiles;

    /**
     * URL of the most recently removed tile. Used to identify when a tile removal is confirmed by
     * the tile backend.
     */
    @Nullable
    private String mPendingRemovalUrl;

    /**
     * URL of the most recently added tile. Used to identify when a given tile's insertion is
     * confirmed by the tile backend. This is relevant when a previously existing tile is removed,
     * then the user undoes the action and wants that tile back.
     */
    @Nullable
    private String mPendingInsertionUrl;

    private boolean mHasReceivedData;

    /**
     * The number of columns tiles get rendered in. Precalculated upon calling
     * {@link #startObserving(int, int)} and constant from then on. Used for pinning the home page
     * tile to the first tile row.
     * @see #renderTiles(ViewGroup)
     */
    private int mNumColumns;

    // TODO(dgn): Attempt to avoid cycling dependencies with TileRenderer. Is there a better way?
    private final TileSetupDelegate mTileSetupDelegate = new TileSetupDelegate() {
        @Override
        public TileInteractionDelegate createInteractionDelegate(Tile tile) {
            return new TileInteractionDelegate(tile.getUrl(), tile.getSectionType());
        }

        @Override
        public LargeIconBridge.LargeIconCallback createIconLoadCallback(Tile tile) {
            // TODO(dgn): We could save on fetches by avoiding a new one when there is one pending
            // for the same URL, and applying the result to all matched URLs.
            boolean trackLoad =
                    isLoadTracked() && tile.getSectionType() == TileSectionType.PERSONALIZED;
            if (trackLoad) addTask(TileTask.FETCH_ICON);
            return new LargeIconCallbackImpl(tile, trackLoad);
        }
    };

    /**
     * @param context Used for initialisation and resolving resources.
     * @param uiDelegate Delegate used to interact with the rest of the system.
     * @param contextMenuManager Used to handle context menu invocations on the tiles.
     * @param tileGroupDelegate Used for interactions with the Most Visited backend.
     * @param observer Will be notified of changes to the tile data.
     * @param titleLines The number of text lines to use for each tile title.
     * @param tileStyle The style to use when building the tiles. See {@link TileView.Style}.
     */
    public TileGroup(Context context, SuggestionsUiDelegate uiDelegate,
            ContextMenuManager contextMenuManager, Delegate tileGroupDelegate, Observer observer,
            OfflinePageBridge offlinePageBridge, int titleLines, @TileView.Style int tileStyle) {
        mUiDelegate = uiDelegate;
        mContextMenuManager = contextMenuManager;
        mTileGroupDelegate = tileGroupDelegate;
        mObserver = observer;
        mTileRenderer =
                new TileRenderer(context, tileStyle, titleLines, uiDelegate.getImageFetcher());

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_OFFLINE_PAGES_FEATURE_NAME)) {
            mOfflineModelObserver = new OfflineModelObserver(offlinePageBridge);
            mUiDelegate.addDestructionObserver(mOfflineModelObserver);
        } else {
            mOfflineModelObserver = null;
        }
    }

    @Override
    public void onSiteSuggestionsAvailable(List<SiteSuggestion> siteSuggestions) {
        // Only transforms the incoming tiles and stores them in a buffer for when we decide to
        // refresh the tiles in the UI.

        boolean removalCompleted = mPendingRemovalUrl != null;
        boolean insertionCompleted = mPendingInsertionUrl == null;

        mPendingTiles = new ArrayList<>();
        for (SiteSuggestion suggestion : siteSuggestions) {
            // The home page tile is pinned to the first row of tiles. It will appear on
            // the position corresponding to its ranking among all tiles (obtained from the
            // ntp_tiles C++ component). If its position is larger than the number of tiles
            // in the first row, it will appear on the last position of the first row.
            // Do note, that the number of tiles in a row (column number) is determined upon
            // initialization and not changed afterwards.
            if (suggestion.source == TileSource.HOMEPAGE) {
                int homeTilePosition = Math.min(mPendingTiles.size(), mNumColumns - 1);
                mPendingTiles.add(homeTilePosition, new Tile(suggestion, homeTilePosition));
            } else {
                mPendingTiles.add(new Tile(suggestion, mPendingTiles.size()));
            }

            // TODO(dgn): Rebase and enable code.
            // Only tiles in the personal section can be modified.
            // if (suggestion.section != TileSectionType.PERSONALIZED) continue;
            if (suggestion.url.equals(mPendingRemovalUrl)) removalCompleted = false;
            if (suggestion.url.equals(mPendingInsertionUrl)) insertionCompleted = true;
        }

        boolean expectedChangeCompleted = false;
        if (mPendingRemovalUrl != null && removalCompleted) {
            mPendingRemovalUrl = null;
            expectedChangeCompleted = true;
        }
        if (mPendingInsertionUrl != null && insertionCompleted) {
            mPendingInsertionUrl = null;
            expectedChangeCompleted = true;
        }

        if (!mHasReceivedData || !mUiDelegate.isVisible() || expectedChangeCompleted) loadTiles();
    }

    @Override
    public void onIconMadeAvailable(String siteUrl) {
        for (Tile tile : findTiles(siteUrl)) {
            mTileRenderer.updateIcon(
                    tile, new LargeIconCallbackImpl(tile, /* trackLoadTask = */ false));
        }
    }

    /**
     * Instructs this instance to start listening for data. The {@link TileGroup.Observer} may be
     * called immediately if new data is received synchronously.
     * @param maxRows The maximum number of rows to fetch.
     * @param maxColumns The maximum number of columns to fetch.
     */
    public void startObserving(int maxRows, int maxColumns) {
        addTask(TileTask.FETCH_DATA);
        mNumColumns = Math.min(maxColumns, TileGridLayout.calculateNumColumns());
        mTileGroupDelegate.setMostVisitedSitesObserver(this, maxRows * maxColumns);
    }

    /**
     * Renders tile views in the given {@link TileGridLayout}.
     * @param parent The layout to render the tile views into.
     */
    public void renderTiles(ViewGroup parent) {
        // TODO(dgn, galinap): Support extra sections in the UI.
        assert mTileSections.size() == 1;
        assert mTileSections.keyAt(0) == TileSectionType.PERSONALIZED;

        for (int i = 0; i < mTileSections.size(); ++i) {
            mTileRenderer.renderTileSection(mTileSections.valueAt(i), parent, mTileSetupDelegate);
        }
        // Icon fetch scheduling was done when building the tile views.
        if (isLoadTracked()) removeTask(TileTask.SCHEDULE_ICON_FETCH);
    }

    /** @deprecated use {@link #getTiles(int)} or {@link #getAllTiles()} instead. */
    @Deprecated
    public Tile[] getTiles() {
        return getTiles(TileSectionType.PERSONALIZED);
    }

    /** @return The tiles currently loaded in the group for the provided section. */
    public Tile[] getTiles(@TileSectionType int section) {
        List<Tile> sectionTiles = mTileSections.get(section);
        if (sectionTiles == null) return new Tile[0];
        return sectionTiles.toArray(new Tile[sectionTiles.size()]);
    }

    /**
     * @return All the tiles currently loaded in the group. Multiple tiles might be returned for
     * the same URL, but they would be in different sections.
     */
    public List<Tile> getAllTiles() {
        List<Tile> tiles = new ArrayList<>();
        for (int i = 0; i < mTileSections.size(); ++i) tiles.addAll(mTileSections.valueAt(i));
        return tiles;
    }

    public boolean hasReceivedData() {
        return mHasReceivedData;
    }

    /**
     * To be called when the view displaying the tile group becomes visible.
     * @param trackLoadTask whether the delegate should be notified that the load is completed
     *                      through {@link Delegate#onLoadingComplete(Tile[])}.
     */
    public void onSwitchToForeground(boolean trackLoadTask) {
        if (trackLoadTask) addTask(TileTask.FETCH_DATA);
        if (mPendingTiles != null) loadTiles();
        if (trackLoadTask) removeTask(TileTask.FETCH_DATA);
    }

    /** Loads tile data from {@link #mPendingTiles} and clears it afterwards. */
    private void loadTiles() {
        assert mPendingTiles != null;

        boolean isInitialLoad = !mHasReceivedData;
        mHasReceivedData = true;

        boolean dataChanged = isInitialLoad;
        List<Tile> personalisedTiles = mTileSections.get(TileSectionType.PERSONALIZED);
        int oldPersonalisedTilesCount = personalisedTiles == null ? 0 : personalisedTiles.size();

        SparseArray<List<Tile>> newSites = createEmptyTileData();
        for (Tile newTile : mPendingTiles) {
            @TileSectionType
            int category = newTile.getSectionType();
            Tile matchingTile = findTile(newTile.getUrl(), category);
            if (matchingTile == null || newTile.importData(matchingTile)) dataChanged = true;

            List<Tile> sectionTiles = newSites.get(category);
            if (sectionTiles == null) {
                sectionTiles = new ArrayList<>(category);
                newSites.append(category, sectionTiles);
            }

            // TODO(dgn): Do we know if it still ever happens? Remove or replace with an assert if
            // it never gets hit. See https://crbug.com/703628
            if (findTile(newTile.getUrl(), sectionTiles) != null) throw new IllegalStateException();

            sectionTiles.add(newTile);
        }

        mTileSections = newSites;
        mPendingTiles = null;

        // TODO(dgn): change these events, maybe introduce new ones or just change semantics? This
        // will depend on the UI to be implemented and the desired refresh behaviour.
        boolean countChanged = isInitialLoad
                || mTileSections.get(TileSectionType.PERSONALIZED).size()
                        != oldPersonalisedTilesCount;
        dataChanged = dataChanged || countChanged;

        if (!dataChanged) return;

        if (mOfflineModelObserver != null) {
            mOfflineModelObserver.updateOfflinableSuggestionsAvailability();
        }

        if (countChanged) mObserver.onTileCountChanged();

        if (isLoadTracked()) addTask(TileTask.SCHEDULE_ICON_FETCH);
        mObserver.onTileDataChanged();

        if (isInitialLoad) removeTask(TileTask.FETCH_DATA);
    }

    /** @return A tile matching the provided URL and section, or {@code null} if none is found. */
    @Nullable
    private Tile findTile(String url, @TileSectionType int section) {
        if (mTileSections.get(section) == null) return null;
        for (Tile tile : mTileSections.get(section)) {
            if (tile.getUrl().equals(url)) return tile;
        }
        return findTile(url, mTileSections.get(section));
    }

    /** @return A tile matching the provided URL and section, or {@code null} if none is found. */
    private Tile findTile(String url, @Nullable List<Tile> tiles) {
        if (tiles == null) return null;
        for (Tile tile : tiles) {
            if (tile.getUrl().equals(url)) return tile;
        }
        return null;
    }

    /** @return All tiles matching the provided URL, or an empty list if none is found. */
    private List<Tile> findTiles(String url) {
        List<Tile> tiles = new ArrayList<>();
        for (int i = 0; i < mTileSections.size(); ++i) {
            for (Tile tile : mTileSections.valueAt(i)) {
                if (tile.getUrl().equals(url)) tiles.add(tile);
            }
        }
        return tiles;
    }

    private void addTask(@TileTask int task) {
        mPendingTasks.add(task);
    }

    private void removeTask(@TileTask int task) {
        boolean removedTask = mPendingTasks.remove(Integer.valueOf(task));
        assert removedTask;

        if (mPendingTasks.isEmpty()) {
            // TODO(dgn): We only notify about the personal tiles because that's the only ones we
            // wait for to be loaded. We also currently rely on the tile order in the returned
            // array as the reported position in UMA, but this is not accurate and would be broken
            // if we returned all the tiles regardless of sections.
            mTileGroupDelegate.onLoadingComplete(getTiles(TileSectionType.PERSONALIZED));
        }
    }

    /**
     * @return Whether the current load is being tracked. Unrequested task tracking updates should
     * not be sent, as it would cause calling {@link Delegate#onLoadingComplete(Tile[])} at the
     * wrong moment.
     */
    private boolean isLoadTracked() {
        return mPendingTasks.contains(TileTask.FETCH_DATA)
                || mPendingTasks.contains(TileTask.SCHEDULE_ICON_FETCH);
    }

    @VisibleForTesting
    boolean isTaskPending(@TileTask int task) {
        return mPendingTasks.contains(task);
    }

    @VisibleForTesting
    TileRenderer getTileRenderer() {
        return mTileRenderer;
    }

    @VisibleForTesting
    TileSetupDelegate getTileSetupDelegate() {
        return mTileSetupDelegate;
    }

    private static SparseArray<List<Tile>> createEmptyTileData() {
        SparseArray<List<Tile>> newTileData = new SparseArray<>();

        // TODO(dgn): How do we want to handle empty states and sections that have no tiles?
        // Have an empty list for now that can be rendered as-is without causing issues or too much
        // state checking. We will have to decide if we want empty lists or no section at all for
        // the others.
        newTileData.put(TileSectionType.PERSONALIZED, new ArrayList<Tile>());

        return newTileData;
    }

    // TODO(dgn): I would like to move that to TileRenderer, but setting the data on the tile,
    // notifying the observer and updating the tasks make it awkward.
    private class LargeIconCallbackImpl implements LargeIconBridge.LargeIconCallback {
        @TileSectionType
        private final int mSection;
        private final String mUrl;
        private final boolean mTrackLoadTask;

        private LargeIconCallbackImpl(Tile tile, boolean trackLoadTask) {
            mUrl = tile.getUrl();
            mSection = tile.getSectionType();
            mTrackLoadTask = trackLoadTask;
        }

        @Override
        public void onLargeIconAvailable(
                @Nullable Bitmap icon, int fallbackColor, boolean isFallbackColorDefault) {
            Tile tile = findTile(mUrl, mSection);
            if (tile != null) { // Do nothing if the tile was removed.
                if (icon == null) {
                    mTileRenderer.setTileIconFromColor(tile, fallbackColor, isFallbackColorDefault);
                } else {
                    mTileRenderer.setTileIconFromBitmap(tile, icon);
                }

                mObserver.onTileIconChanged(tile);
            }

            // This call needs to be made after the tiles are completely initialised, for UMA.
            if (mTrackLoadTask) removeTask(TileTask.FETCH_ICON);
        }
    }

    /**
     * Implements various listener and delegate interfaces to handle user interactions with tiles.
     */
    public class TileInteractionDelegate
            implements ContextMenuManager.Delegate, OnClickListener, OnCreateContextMenuListener {
        @TileSectionType
        private final int mSection;
        private final String mUrl;

        public TileInteractionDelegate(String url, int section) {
            mUrl = url;
            mSection = section;
        }

        @Override
        public void onClick(View view) {
            Tile tile = findTile(mUrl, mSection);
            if (tile == null) return;

            SuggestionsMetrics.recordTileTapped();
            mTileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.CURRENT_TAB, tile);
        }

        @Override
        public void openItem(int windowDisposition) {
            Tile tile = findTile(mUrl, mSection);
            if (tile == null) return;

            mTileGroupDelegate.openMostVisitedItem(windowDisposition, tile);
        }

        @Override
        public void removeItem() {
            Tile tile = findTile(mUrl, mSection);
            if (tile == null) return;

            // Note: This does not track all the removals, but will track the most recent one. If
            // that removal is committed, it's good enough for change detection.
            mPendingRemovalUrl = mUrl;
            mTileGroupDelegate.removeMostVisitedItem(tile, new RemovalUndoneCallback());
        }

        @Override
        public String getUrl() {
            return mUrl;
        }

        @Override
        public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
            return true;
        }

        @Override
        public void onContextMenuCreated() {}

        @Override
        public void onCreateContextMenu(
                ContextMenu contextMenu, View view, ContextMenuInfo contextMenuInfo) {
            mContextMenuManager.createContextMenu(contextMenu, view, this);
        }
    }

    private class RemovalUndoneCallback extends Callback<String> {
        @Override
        public void onResult(String restoredUrl) {
            mPendingInsertionUrl = restoredUrl;
        }
    }

    private class OfflineModelObserver extends SuggestionsOfflineModelObserver<Tile> {
        public OfflineModelObserver(OfflinePageBridge bridge) {
            super(bridge);
        }

        @Override
        public void onSuggestionOfflineIdChanged(Tile tile, @Nullable Long id) {
            boolean oldOfflineAvailable = tile.isOfflineAvailable();
            tile.setOfflinePageOfflineId(id);

            // Only notify to update the view if there will be a visible change.
            if (oldOfflineAvailable == tile.isOfflineAvailable()) return;
            mObserver.onTileOfflineBadgeVisibilityChanged(tile);
        }

        @Override
        public Iterable<Tile> getOfflinableSuggestions() {
            return getAllTiles();
        }
    }
}
