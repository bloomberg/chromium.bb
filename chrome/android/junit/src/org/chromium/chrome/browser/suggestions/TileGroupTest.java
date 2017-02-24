// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static junit.framework.TestCase.assertNotNull;

import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;
import android.support.annotation.ColorRes;
import android.support.annotation.DimenRes;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadows.ShadowResources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.EnableFeatures;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.NodeParent;
import org.chromium.chrome.browser.ntp.cards.SuggestionsSection;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for {@link TileGroup}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {TileGroupTest.TileShadowResources.class})
public class TileGroupTest {
    private static final int MAX_TILES_TO_FETCH = 4;
    private static final int TILE_TITLE_LINES = 1;
    private static final String[] URLS = {"https://www.google.com", "https://tellmedadjokes.com"};

    @Rule
    public EnableFeatures.Processor mEnableFeatureProcessor = new EnableFeatures.Processor();

    @Mock
    private SuggestionsSection.Delegate mDelegate;
    @Mock
    private NodeParent mParent;
    @Mock
    private SuggestionsUiDelegate mUiDelegate;
    @Mock
    private TileGroup.Observer mTileGroupObserver;
    @Mock
    private OfflinePageBridge mOfflinePageBridge;

    private FakeTileGroupDelegate mTileGroupDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTileGroupDelegate = new FakeTileGroupDelegate();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_OFFLINE_PAGES_FEATURE_NAME})
    public void testInitialiseWithEmptyTileList() {
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, mUiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mOfflinePageBridge, TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        notifyTileUrlsAvailable();

        verify(mTileGroupObserver).onTileCountChanged();
        verify(mTileGroupObserver).onLoadTaskCompleted();
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_OFFLINE_PAGES_FEATURE_NAME})
    public void testInitialiseWithTileList() {
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, mUiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mOfflinePageBridge, TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        notifyTileUrlsAvailable(URLS);

        InOrder inOrder = inOrder(mTileGroupObserver);
        inOrder.verify(mTileGroupObserver).onTileCountChanged();
        inOrder.verify(mTileGroupObserver).onLoadTaskCompleted();
        inOrder.verify(mTileGroupObserver).onTileDataChanged();
        inOrder.verifyNoMoreInteractions();
    }

    private void notifyTileUrlsAvailable(String... urls) {
        assertNotNull("MostVisitedObserver has not been registered.", mTileGroupDelegate.mObserver);

        String[] titles = urls.clone();
        String[] whitelistIconPaths = new String[urls.length];
        int[] sources = new int[urls.length];
        for (int i = 0; i < urls.length; ++i) whitelistIconPaths[i] = "";
        mTileGroupDelegate.mObserver.onMostVisitedURLsAvailable(
                titles, urls, whitelistIconPaths, sources);
    }

    private class FakeTileGroupDelegate implements TileGroup.Delegate {
        public MostVisitedSites.Observer mObserver;

        @Override
        public void removeMostVisitedItem(Tile tile) {}

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

    /**
     * Replacement for the {@link Resources} to allow loading resources used by {@link TileGroup} in
     * unit tests.
     * TODO(https://crbug.com/693573): Needed until unit tests can pick up resources themselves.
     */
    @Implements(Resources.class)
    public static class TileShadowResources extends ShadowResources {
        @RealObject
        private Resources mRealResources;
        public int getDimensionPixelSize(@DimenRes int id) {
            if (id == R.dimen.tile_view_icon_size) return 2;
            return mRealResources.getDimensionPixelSize(id);
        }

        @SuppressWarnings("deprecation")
        public int getColor(@ColorRes int id) {
            if (id == R.color.default_favicon_background_color) return 2;
            return mRealResources.getColor(id);
        }
    }
}
