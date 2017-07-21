// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.hamcrest.CoreMatchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.CardsVariationParameters;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Unit tests for {@link TileGroup}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features({@Features.Register(ChromeFeatureList.NTP_OFFLINE_PAGES_FEATURE_NAME),
        @Features.Register(ChromeFeatureList.SUGGESTIONS_HOME_MODERN_LAYOUT)})
public class TileGroupTest {
    private static final int MAX_COLUMNS_TO_FETCH = 4;
    private static final int MAX_ROWS_TO_FETCH = 1;
    private static final int TILE_TITLE_LINES = 1;
    private static final String[] URLS = {"https://www.google.com", "https://tellmedadjokes.com"};

    @Rule
    public Features.Processor mFeaturesProcessor = new Features.Processor();

    @Mock
    private TileGroup.Observer mTileGroupObserver;

    private FakeTileGroupDelegate mTileGroupDelegate;
    private FakeImageFetcher mImageFetcher;

    @Before
    public void setUp() {
        CardsVariationParameters.setTestVariationParams(new HashMap<String, String>());

        MockitoAnnotations.initMocks(this);
        mTileGroupDelegate = spy(new FakeTileGroupDelegate());
        mImageFetcher = new FakeImageFetcher();
    }

    @After
    public void tearDown() {
        CardsVariationParameters.setTestVariationParams(null);
    }

    @Test
    public void testInitialiseWithTileList() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_ROWS_TO_FETCH, MAX_COLUMNS_TO_FETCH);

        notifyTileUrlsAvailable(URLS);

        verify(mTileGroupObserver).onTileCountChanged();
        verify(mTileGroupObserver).onTileDataChanged();

        verify(mTileGroupDelegate, never()).onLoadingComplete(any(Tile[].class));
        assertTrue(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.FETCH_DATA));
    }

    /**
     * Test the special case during initialisation, where we notify TileGroup.Observer of changes
     * event though the data did not change (still empty just like before initialisation).
     */
    @Test
    public void testInitialiseWithEmptyTileList() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_ROWS_TO_FETCH, MAX_COLUMNS_TO_FETCH);

        notifyTileUrlsAvailable(/* nothing! */);

        verify(mTileGroupObserver).onTileCountChanged();
        verify(mTileGroupObserver).onTileDataChanged();

        verify(mTileGroupDelegate, never()).onLoadingComplete(any(Tile[].class));
        assertTrue(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.FETCH_DATA));
    }

    @Test
    public void testReceiveNewTilesWithoutChanges() {
        TileGroup tileGroup = initialiseTileGroup(URLS);

        // Notify the same thing. No changes so|mTileGroupObserver| should not be notified.
        notifyTileUrlsAvailable(URLS);

        verifyNoMoreInteractions(mTileGroupObserver);
        verifyNoMoreInteractions(mTileGroupDelegate);
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testReceiveNewTilesWithoutChanges_TrackLoad() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true, URLS);

        // Notify the same thing. No changes so|mTileGroupObserver| should not be notified.
        notifyTileUrlsAvailable(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ true);

        verifyNoMoreInteractions(mTileGroupObserver);
        verifyNoMoreInteractions(mTileGroupDelegate);
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testReceiveNewTilesWithDataChanges() {
        TileGroup tileGroup = initialiseTileGroup(URLS);

        // Notify the about different URLs, but the same number. #onTileCountChanged() should not be
        // called.
        notifyTileUrlsAvailable("foo", "bar");

        verify(mTileGroupObserver, never()).onTileCountChanged(); // Tile count is still 2.
        verify(mTileGroupObserver).onTileDataChanged(); // Data DID change.

        // No load task the second time.
        verify(mTileGroupDelegate, never()).onLoadingComplete(any(Tile[].class));
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testReceiveNewTilesWithDataChanges_TrackLoad() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true, URLS);

        // Notify the about different URLs, but the same number. #onTileCountChanged() should not be
        // called.
        notifyTileUrlsAvailable("foo", "bar");
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ true);

        verify(mTileGroupObserver).onTileDataChanged(); // Now data DID change.
        verify(mTileGroupObserver, never()).onTileCountChanged(); // Tile count is still 2.

        // We should now have a pending task.
        verify(mTileGroupDelegate, never()).onLoadingComplete(any(Tile[].class));
        assertTrue(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testReceiveNewTilesWithCountChanges() {
        TileGroup tileGroup = initialiseTileGroup(URLS);

        notifyTileUrlsAvailable(URLS[0]);

        verify(mTileGroupObserver).onTileCountChanged(); // Tile count DID change.
        verify(mTileGroupObserver).onTileDataChanged(); // Data DID change.
        verify(mTileGroupDelegate, never())
                .onLoadingComplete(any(Tile[].class)); // No load task the second time.
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testTileLoadingWhenVisibleNotBlockedForInit() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.isVisible()).thenReturn(true);
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, uiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_ROWS_TO_FETCH, MAX_COLUMNS_TO_FETCH);

        notifyTileUrlsAvailable(URLS);

        // Because it's the first load, we accept the incoming tiles and refresh the view.
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testTileLoadingWhenVisibleBlocked() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.isVisible()).thenReturn(true);
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, uiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_ROWS_TO_FETCH, MAX_COLUMNS_TO_FETCH);

        notifyTileUrlsAvailable(URLS);
        reset(mTileGroupObserver);

        notifyTileUrlsAvailable(URLS[0]);

        // Even though the data changed, the notification should not happen because we want to not
        // show changes to UI elements currently visible
        verify(mTileGroupObserver, never()).onTileDataChanged();

        // Simulating a switch from background to foreground should force the tilegrid to load the
        // new data.
        tileGroup.onSwitchToForeground(true);
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testTileLoadingWhenVisibleBlocked_2() {
        TileGroup tileGroup = initialiseTileGroup(true, URLS);

        notifyTileUrlsAvailable(URLS[0]);

        // Even though the data changed, the notification should not happen because we want to not
        // show changes to UI elements currently visible
        verify(mTileGroupObserver, never()).onTileDataChanged();

        // Simulating a switch from background to foreground should force the tilegrid to load the
        // new data.
        tileGroup.onSwitchToForeground(true);
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testRenderTileView() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.getImageFetcher()).thenReturn(mock(ImageFetcher.class));
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, uiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_ROWS_TO_FETCH, MAX_COLUMNS_TO_FETCH);
        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);

        // Initialise the internal list of tiles
        notifyTileUrlsAvailable(URLS);

        // Render them to the layout.
        tileGroup.renderTileViews(layout, false);
        assertThat(layout.getChildCount(), is(2));
        assertThat(((TileView) layout.getChildAt(0)).getUrl(), is(URLS[0]));
        assertThat(((TileView) layout.getChildAt(1)).getUrl(), is(URLS[1]));
    }

    /** Check for https://crbug.com/703628: handle duplicated URLs by ignoring them. */
    @Test
    public void testRenderTileViewWithDuplicatedUrl() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.getImageFetcher()).thenReturn(mock(ImageFetcher.class));
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, uiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_ROWS_TO_FETCH, MAX_COLUMNS_TO_FETCH);
        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);

        // Initialise the internal list of tiles
        notifyTileUrlsAvailable(URLS[0], URLS[1], URLS[0]);

        // Render them to the layout. The duplicated URL is skipped.
        tileGroup.renderTileViews(layout, false);
        assertThat(layout.getChildCount(), is(2));
        assertThat(((TileView) layout.getChildAt(0)).getUrl(), is(URLS[0]));
        assertThat(((TileView) layout.getChildAt(1)).getUrl(), is(URLS[1]));
    }

    @Test
    public void testRenderTileViewReplacing() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.getImageFetcher()).thenReturn(mock(ImageFetcher.class));
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, uiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_ROWS_TO_FETCH, MAX_COLUMNS_TO_FETCH);
        notifyTileUrlsAvailable(URLS);

        // Initialise the layout with views whose URLs don't match the ones of the new tiles.
        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        TileView view1 = mock(TileView.class);
        layout.addView(view1);

        TileView view2 = mock(TileView.class);
        layout.addView(view2);

        // The tiles should be updated, the old ones removed.
        tileGroup.renderTileViews(layout, false);
        assertThat(layout.getChildCount(), is(2));
        assertThat(layout.indexOfChild(view1), is(-1));
        assertThat(layout.indexOfChild(view2), is(-1));
        verify(view1, never()).updateIfDataChanged(tileGroup.getTiles()[0]);
        verify(view2, never()).updateIfDataChanged(tileGroup.getTiles()[1]);
    }

    @Test
    public void testRenderTileViewRecycling() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_ROWS_TO_FETCH, MAX_COLUMNS_TO_FETCH);
        notifyTileUrlsAvailable(URLS);

        // Initialise the layout with views whose URLs match the ones of the new tiles.
        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        TileView view1 = mock(TileView.class);
        when(view1.getUrl()).thenReturn(URLS[0]);
        layout.addView(view1);

        TileView view2 = mock(TileView.class);
        when(view2.getUrl()).thenReturn(URLS[1]);
        layout.addView(view2);

        // The tiles should be updated, the old ones reused.
        tileGroup.renderTileViews(layout, false);
        assertThat(layout.getChildCount(), is(2));
        assertThat(layout.getChildAt(0), CoreMatchers.<View>is(view1));
        assertThat(layout.getChildAt(1), CoreMatchers.<View>is(view2));
        verify(view1).updateIfDataChanged(tileGroup.getTiles()[0]);
        verify(view2).updateIfDataChanged(tileGroup.getTiles()[1]);
    }

    @Test
    public void testIconLoadingForInit() {
        TileGroup tileGroup = initialiseTileGroup(URLS);
        Tile tile = tileGroup.getTiles()[0];

        // Loading complete should be delayed until the icons are done loading.
        verify(mTileGroupDelegate, never()).onLoadingComplete(any(Tile[].class));

        mImageFetcher.fulfillLargeIconRequests();

        // The load should now be complete.
        verify(mTileGroupDelegate).onLoadingComplete(any(Tile[].class));
        verify(mTileGroupObserver).onTileIconChanged(eq(tile));
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
    }

    @Test
    public void testIconLoadingWhenTileNotRegistered() {
        TileGroup tileGroup = initialiseTileGroup();
        Tile tile = new Tile("title", URLS[0], "", 0, TileSource.POPULAR);

        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        tileGroup.buildTileView(tile, layout, /* condensed = */ false);

        // Ensure we run the callback for the new tile.
        assertEquals(1, mImageFetcher.getPendingIconCallbackCount());
        mImageFetcher.fulfillLargeIconRequests();

        verify(mTileGroupObserver, never()).onTileIconChanged(tile);
    }

    @Test
    public void testIconLoading_Sync() {
        TileGroup tileGroup = initialiseTileGroup();
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        notifyTileUrlsAvailable(URLS);
        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        tileGroup.renderTileViews(layout, /* condensed: */ false);
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate, never()).onLoadingComplete(any(Tile[].class));
    }

    @Test
    public void testIconLoading_AsyncNoTrack() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true);
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        notifyTileUrlsAvailable(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ false);
        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        tileGroup.renderTileViews(layout, /* condensed: */ false);
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent (same as sync)
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate, never()).onLoadingComplete(any(Tile[].class));
    }

    @Test
    public void testIconLoading_AsyncTrack() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true);
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        notifyTileUrlsAvailable(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ true);
        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        tileGroup.renderTileViews(layout, /* condensed: */ false);
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate).onLoadingComplete(any(Tile[].class));
    }

    /**
     * Notifies the tile group of new tiles created from the provided URLs. Requires
     * {@link TileGroup#startObserving(int, int)} to have been called on the tile group under test.
     * @see TileGroup#onMostVisitedURLsAvailable(String[], String[], String[], int[])
     */
    private void notifyTileUrlsAvailable(String... urls) {
        assertNotNull("MostVisitedObserver has not been registered.", mTileGroupDelegate.mObserver);

        String[] titles = urls.clone();
        String[] whitelistIconPaths = new String[urls.length];
        int[] sources = new int[urls.length];
        for (int i = 0; i < urls.length; ++i) whitelistIconPaths[i] = "";
        mTileGroupDelegate.mObserver.onMostVisitedURLsAvailable(
                titles, urls, whitelistIconPaths, sources);
    }

    /** {@link #initialiseTileGroup(boolean, String...)} override that does not defer loads. */
    private TileGroup initialiseTileGroup(String... urls) {
        return initialiseTileGroup(false, urls);
    }

    /**
     * @param deferLoad whether to defer the load until
     *                  {@link TileGroup#onSwitchToForeground(boolean)} is called. Works by
     *                  pretending that the UI is visible.
     * @param urls URLs used to initialise the tile group.
     */
    private TileGroup initialiseTileGroup(boolean deferLoad, String... urls) {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.getImageFetcher()).thenReturn(mImageFetcher);
        when(uiDelegate.isVisible()).thenReturn(deferLoad);

        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, uiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_ROWS_TO_FETCH, MAX_COLUMNS_TO_FETCH);
        notifyTileUrlsAvailable(urls);

        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        tileGroup.renderTileViews(layout, /* condensed = */ false);

        reset(mTileGroupObserver);
        reset(mTileGroupDelegate);
        return tileGroup;
    }

    private class FakeTileGroupDelegate implements TileGroup.Delegate {
        public MostVisitedSites.Observer mObserver;

        @Override
        public void removeMostVisitedItem(Tile tile, Callback<String> removalUndoneCallback) {}

        @Override
        public void openMostVisitedItem(int windowDisposition, Tile tile) {}

        @Override
        public void setMostVisitedSitesObserver(
                MostVisitedSites.Observer observer, int maxResults) {
            mObserver = observer;
        }

        @Override
        public void onLoadingComplete(Tile[] tiles) {}

        @Override
        public void destroy() {}
    }

    private class FakeImageFetcher extends ImageFetcher {
        private final List<LargeIconCallback> mCallbackList = new ArrayList<>();

        public FakeImageFetcher() {
            super(null, null, null);
        }

        @Override
        public void makeLargeIconRequest(String url, int size, LargeIconCallback callback) {
            mCallbackList.add(callback);
        }

        public void fulfillLargeIconRequests(Bitmap bitmap, int color, boolean isColorDefault) {
            for (LargeIconCallback callback : mCallbackList) {
                callback.onLargeIconAvailable(bitmap, color, isColorDefault);
            }
            mCallbackList.clear();
        }

        public int getPendingIconCallbackCount() {
            return mCallbackList.size();
        }

        public void fulfillLargeIconRequests() {
            fulfillLargeIconRequests(mock(Bitmap.class), Color.BLACK, false);
        }
    }
}
