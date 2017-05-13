// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.hamcrest.CoreMatchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for {@link TileGroup}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features(@Features.Register(ChromeFeatureList.NTP_OFFLINE_PAGES_FEATURE_NAME))
public class TileGroupTest {
    private static final int MAX_TILES_TO_FETCH = 4;
    private static final int TILE_TITLE_LINES = 1;
    private static final String[] URLS = {"https://www.google.com", "https://tellmedadjokes.com"};

    @Rule
    public Features.Processor mFeaturesProcessor = new Features.Processor();

    @Mock
    private TileGroup.Observer mTileGroupObserver;

    private FakeTileGroupDelegate mTileGroupDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTileGroupDelegate = new FakeTileGroupDelegate();
    }

    @Test
    public void testInitialiseWithEmptyTileList() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        notifyTileUrlsAvailable();

        // The TileGroup.Observer methods should be called even though no tiles are added, which is
        // an initialisation but not real state change.
        verify(mTileGroupObserver).onTileCountChanged();
        verify(mTileGroupObserver).onLoadTaskCompleted();
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testInitialiseWithTileList() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        notifyTileUrlsAvailable(URLS);

        verify(mTileGroupObserver).onTileCountChanged();
        verify(mTileGroupObserver).onLoadTaskCompleted();
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testReceiveNewTilesWithoutChanges() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        // First initialisation
        notifyTileUrlsAvailable(URLS);
        reset(mTileGroupObserver);

        // Notify the same thing. No changes so|mTileGroupObserver| should not be notified.
        notifyTileUrlsAvailable(URLS);
        verifyNoMoreInteractions(mTileGroupObserver);
    }

    @Test
    public void testReceiveNewTilesWithDataChanges() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        // First initialisation
        notifyTileUrlsAvailable(URLS);
        reset(mTileGroupObserver);

        // Notify the about different URLs, but the same number. #onTileCountChanged() should not be
        // called.
        notifyTileUrlsAvailable("foo", "bar");
        verify(mTileGroupObserver, never()).onTileCountChanged(); // Tile count is still 2
        verify(mTileGroupObserver, never()).onLoadTaskCompleted(); // No load task the second time.
        verify(mTileGroupObserver).onTileDataChanged(); // Data DID change.
    }

    @Test
    public void testReceiveNewTilesWithCountChanges() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        // First initialisation
        notifyTileUrlsAvailable(URLS);
        reset(mTileGroupObserver);

        notifyTileUrlsAvailable(URLS[0]);
        verify(mTileGroupObserver).onTileCountChanged(); // Tile count DID change.
        verify(mTileGroupObserver, never()).onLoadTaskCompleted(); // No load task the second time.
        verify(mTileGroupObserver).onTileDataChanged(); // Data DID change.
    }

    @Test
    public void testTileLoadingWhenVisibleNotBlockedForInit() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.isVisible()).thenReturn(true);
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, uiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

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
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        notifyTileUrlsAvailable(URLS);
        reset(mTileGroupObserver);
        notifyTileUrlsAvailable(URLS[0]);

        // Even though the data changed, the notification should not happen because we want to not
        // show changes to UI elements currently visible
        verify(mTileGroupObserver, never()).onTileDataChanged();

        // Simulating a switch from background to foreground should force the tilegrid to load the
        // new data.
        tileGroup.onSwitchToForeground();
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testRenderTileView() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);

        // Initialise the internal list of tiles
        notifyTileUrlsAvailable(URLS);

        // Render them to the layout.
        tileGroup.renderTileViews(layout, false, false);
        assertThat(layout.getChildCount(), is(2));
        assertThat(((TileView) layout.getChildAt(0)).getUrl(), is(URLS[0]));
        assertThat(((TileView) layout.getChildAt(1)).getUrl(), is(URLS[1]));
    }

    @Test
    public void testRenderTileViewWithDuplicatedUrl() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);

        // Initialise the internal list of tiles
        notifyTileUrlsAvailable(URLS[0], URLS[1], URLS[0]);

        // Render them to the layout. The duplicated URL is skipped.
        tileGroup.renderTileViews(layout, false, false);
        assertThat(layout.getChildCount(), is(2));
        assertThat(((TileView) layout.getChildAt(0)).getUrl(), is(URLS[0]));
        assertThat(((TileView) layout.getChildAt(1)).getUrl(), is(URLS[1]));
    }

    @Test
    public void testRenderTileViewReplacing() {
        TileGroup tileGroup =
                new TileGroup(RuntimeEnvironment.application, mock(SuggestionsUiDelegate.class),
                        mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                        mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        notifyTileUrlsAvailable(URLS);

        // Initialise the layout with views whose URLs don't match the ones of the new tiles.
        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        TileView view1 = mock(TileView.class);
        layout.addView(view1);

        TileView view2 = mock(TileView.class);
        layout.addView(view2);

        // The tiles should be updated, the old ones removed.
        tileGroup.renderTileViews(layout, false, false);
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
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
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
        tileGroup.renderTileViews(layout, false, false);
        assertThat(layout.getChildCount(), is(2));
        assertThat(layout.getChildAt(0), CoreMatchers.<View>is(view1));
        assertThat(layout.getChildAt(1), CoreMatchers.<View>is(view2));
        verify(view1).updateIfDataChanged(tileGroup.getTiles()[0]);
        verify(view2).updateIfDataChanged(tileGroup.getTiles()[1]);
    }

    @Test
    public void testIconLoading() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, uiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        notifyTileUrlsAvailable(URLS); // Initialise the internal state to include the test tile.
        reset(mTileGroupObserver);
        Tile tile = tileGroup.getTiles()[0];

        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        tileGroup.buildTileView(tile, layout, /* trackLoadTask = */ true, /* condensed = */ false);

        verify(mTileGroupObserver).onLoadTaskAdded();

        ArgumentCaptor<LargeIconCallback> captor = ArgumentCaptor.forClass(LargeIconCallback.class);
        verify(uiDelegate).getLargeIconForUrl(any(String.class), anyInt(), captor.capture());
        for (LargeIconCallback cb : captor.getAllValues()) {
            cb.onLargeIconAvailable(mock(Bitmap.class), Color.BLACK, /* isColorDefault = */ false);
        }

        verify(mTileGroupObserver).onLoadTaskCompleted();
        verify(mTileGroupObserver).onTileIconChanged(tile);
    }

    @Test
    public void testIconLoadingNoTask() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, uiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        notifyTileUrlsAvailable(URLS); // Initialise the internal state to include the test tile.
        reset(mTileGroupObserver);
        Tile tile = tileGroup.getTiles()[0];

        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        tileGroup.buildTileView(tile, layout, /* trackLoadTask = */ false, /* condensed = */ false);

        verify(mTileGroupObserver, never()).onLoadTaskAdded();

        ArgumentCaptor<LargeIconCallback> captor = ArgumentCaptor.forClass(LargeIconCallback.class);
        verify(uiDelegate).getLargeIconForUrl(any(String.class), anyInt(), captor.capture());
        for (LargeIconCallback cb : captor.getAllValues()) {
            cb.onLargeIconAvailable(mock(Bitmap.class), Color.BLACK, /* isColorDefault = */ false);
        }

        verify(mTileGroupObserver, never()).onLoadTaskCompleted();
        verify(mTileGroupObserver).onTileIconChanged(tile);
    }

    @Test
    public void testIconLoadingWhenTileNotRegistered() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, uiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class), TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        reset(mTileGroupObserver);
        Tile tile = new Tile("title", URLS[0], "", 0, TileSource.POPULAR);

        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        tileGroup.buildTileView(tile, layout, /* trackLoadTask = */ true, /* condensed = */ false);
        verify(mTileGroupObserver).onLoadTaskAdded();

        ArgumentCaptor<LargeIconCallback> captor = ArgumentCaptor.forClass(LargeIconCallback.class);
        verify(uiDelegate).getLargeIconForUrl(any(String.class), anyInt(), captor.capture());
        captor.getValue().onLargeIconAvailable(mock(Bitmap.class), Color.BLACK, false);

        verify(mTileGroupObserver).onLoadTaskCompleted();
        verify(mTileGroupObserver, never()).onTileIconChanged(tile);
    }

    /**
     * Notifies the tile group of new tiles created from the provided URLs. Requires
     * {@link TileGroup#startObserving(int)} to have been called on the tile group under test.
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

    /**
     * Creates a mocked {@link TileView} that will return the URL of the tile it has been
     * initialised with.
     */
    private static TileView createMockTileView() {
        final TileView tileView = mock(TileView.class);
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Tile tile = invocation.getArgument(0);
                when(tileView.getUrl()).thenReturn(tile.getUrl());
                return null;
            }
        })
                .when(tileView)
                .initialize(any(Tile.class), anyInt(), anyBoolean());
        return tileView;
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
}
