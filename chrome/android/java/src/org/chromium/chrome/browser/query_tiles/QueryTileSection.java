// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.ntp.search.SearchBoxChipDelegate;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinator;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinatorFactory;
import org.chromium.components.browser_ui.widget.image_tiles.TileConfig;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.components.query_tiles.QueryTileConstants;
import org.chromium.components.query_tiles.TileProvider;
import org.chromium.components.query_tiles.TileUmaLogger;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Represents the query tiles section on the new tab page. Abstracts away the general tasks related
 * to initializing and fetching data for the UI and making decisions whether to show or hide this
 * section.
 */
public class QueryTileSection {
    private static final String MOST_VISITED_MAX_ROWS_SMALL_SCREEN =
            "most_visited_max_rows_small_screen";
    private static final String MOST_VISITED_MAX_ROWS_NORMAL_SCREEN =
            "most_visited_max_rows_normal_screen";
    private static final String VARIATION_SMALL_SCREEN_HEIGHT_THRESHOLD_DP =
            "small_screen_height_threshold_dp";
    private static final int DEFAULT_SMALL_SCREEN_HEIGHT_THRESHOLD_DP = 700;
    private static final String UMA_PREFIX = "QueryTiles.NTP";

    private final ViewGroup mQueryTileSectionView;
    private final SearchBoxCoordinator mSearchBoxCoordinator;
    private final Callback<QueryInfo> mSubmitQueryCallback;
    private ImageTileCoordinator mTileCoordinator;
    private TileProvider mTileProvider;
    private TileUmaLogger mTileUmaLogger;
    private ImageFetcher mImageFetcher;
    private Integer mTileWidth;
    private float mAnimationPercent;

    /**
     * Represents the information needed to launch a search query when clicking on a tile.
     */
    public static class QueryInfo {
        public final String queryText;
        public final List<String> searchParams;

        public QueryInfo(String queryText, List<String> searchParams) {
            this.queryText = queryText;
            this.searchParams = searchParams;
        }
    }

    /** Constructor. */
    public QueryTileSection(ViewGroup queryTileSectionView,
            SearchBoxCoordinator searchBoxCoordinator, Profile profile,
            Callback<QueryInfo> performSearchQueryCallback) {
        mQueryTileSectionView = queryTileSectionView;
        mSearchBoxCoordinator = searchBoxCoordinator;
        mSubmitQueryCallback = performSearchQueryCallback;

        mTileProvider = TileProviderFactory.getForProfile(profile);
        TileConfig tileConfig = new TileConfig.Builder().setUmaPrefix(UMA_PREFIX).build();
        mTileUmaLogger = new TileUmaLogger(UMA_PREFIX);
        mTileCoordinator = ImageTileCoordinatorFactory.create(mQueryTileSectionView.getContext(),
                tileConfig, this::onTileClicked, this::getVisuals);
        mQueryTileSectionView.addView(mTileCoordinator.getView(),
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mImageFetcher =
                ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        GlobalDiscardableReferencePool.getReferencePool());
        mSearchBoxCoordinator.addVoiceSearchButtonClickListener(v -> reloadTiles());
        reloadTiles();
    }

    /**
     * Called to notify the animation progress for transition between fake search box and omnibox.
     * @param percent The animation progress.
     */
    public void onUrlFocusAnimationChanged(float percent) {
        if (mAnimationPercent == percent) return;
        mAnimationPercent = percent;
        if (percent == 0) reloadTiles();
    }

    /**
     * Called to clear the state and reload the top level tiles. Any chip selected will be cleared.
     */
    public void reloadTiles() {
        mTileProvider.getQueryTiles(this::setTiles);
        mSearchBoxCoordinator.setChipText(null);
    }

    private void onTileClicked(ImageTile tile) {
        QueryTile queryTile = (QueryTile) tile;
        mTileUmaLogger.recordTileClicked(queryTile);

        boolean isLastLevelTile = queryTile.children.isEmpty();
        if (isLastLevelTile) {
            if (QueryTileUtils.isQueryEditingEnabled()) {
                mSearchBoxCoordinator.setSearchText(queryTile.queryText);
            } else {
                mSubmitQueryCallback.onResult(
                        new QueryInfo(queryTile.queryText, queryTile.searchParams));
            }
            return;
        }

        setTiles(queryTile.children);
        showQueryChip(queryTile);
    }

    private void showQueryChip(QueryTile queryTile) {
        mSearchBoxCoordinator.setChipText(queryTile.queryText);
        mSearchBoxCoordinator.setChipDelegate(new SearchBoxChipDelegate() {
            @Override
            public void onChipClicked() {
                mTileUmaLogger.recordSearchButtonClicked(queryTile);
                mSubmitQueryCallback.onResult(
                        new QueryInfo(queryTile.queryText, queryTile.searchParams));
            }

            @Override
            public void onCancelClicked() {
                mTileUmaLogger.recordChipCleared();
                mSearchBoxCoordinator.setChipText(null);
                reloadTiles();
            }

            @Override
            public void getChipIcon(Callback<Bitmap> callback) {
                int chipIconSize = mQueryTileSectionView.getResources().getDimensionPixelSize(
                        R.dimen.chip_icon_size);
                fetchImage(queryTile, chipIconSize, callback);
            }
        });
    }

    private void setTiles(List<QueryTile> tiles) {
        mTileUmaLogger.recordTilesLoaded(tiles);
        mTileCoordinator.setTiles(new ArrayList<>(tiles));
        mQueryTileSectionView.setVisibility(tiles.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private void getVisuals(ImageTile tile, Callback<List<Bitmap>> callback) {
        // TODO(crbug.com/1077086): Probably need a bigger width to start with or pass the exact
        // width. Also may need to update on orientation change.
        if (mTileWidth == null) {
            mTileWidth = mQueryTileSectionView.getResources().getDimensionPixelSize(
                    R.dimen.tile_ideal_width);
        }

        fetchImage((QueryTile) tile, mTileWidth,
                bitmap -> { callback.onResult(Arrays.asList(bitmap)); });
    }

    private void fetchImage(QueryTile queryTile, int size, Callback<Bitmap> callback) {
        if (queryTile.urls.isEmpty()) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(null));
            return;
        }

        String url = queryTile.urls.get(0);
        ImageFetcher.Params params = ImageFetcher.Params.createWithExpirationInterval(url,
                ImageFetcher.QUERY_TILE_UMA_CLIENT_NAME, size, size,
                QueryTileConstants.IMAGE_EXPIRATION_INTERVAL_MINUTES);
        mImageFetcher.fetchImage(params, callback);
    }

    /**
     * @return Max number of rows for most visited tiles. For smaller screens, the most visited
     *         tiles section on NTP is shortened so that feed is still visible above the fold.
     */
    public Integer getMaxRowsForMostVisitedTiles() {
        DisplayAndroid display =
                DisplayAndroid.getNonMultiDisplay(mQueryTileSectionView.getContext());
        int screenHeightDp = DisplayUtil.pxToDp(display, display.getDisplayHeight());
        int smallScreenHeightThresholdDp = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.QUERY_TILES, VARIATION_SMALL_SCREEN_HEIGHT_THRESHOLD_DP,
                DEFAULT_SMALL_SCREEN_HEIGHT_THRESHOLD_DP);
        boolean isSmallScreen = screenHeightDp < smallScreenHeightThresholdDp;
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(ChromeFeatureList.QUERY_TILES,
                isSmallScreen ? MOST_VISITED_MAX_ROWS_SMALL_SCREEN
                              : MOST_VISITED_MAX_ROWS_NORMAL_SCREEN,
                2);
    }
}
