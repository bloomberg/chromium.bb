// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static junit.framework.TestCase.assertNotNull;

import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;
import android.support.annotation.ColorRes;
import android.support.annotation.DimenRes;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadows.ShadowResources;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.NodeParent;
import org.chromium.chrome.browser.ntp.cards.SuggestionsSection;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Collections;
import java.util.Set;

/**
 * Unit tests for {@link TileGroup}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {TileGroupTest.TileShadowResources.class})
public class TileGroupTest {
    private static final int MAX_TILES_TO_FETCH = 4;
    private static final int TILE_TITLE_LINES = 1;
    private static final String[] URLS = {"https://www.google.com", "https://tellmedadjokes.com"};

    @Mock
    private SuggestionsSection.Delegate mDelegate;
    @Mock
    private NodeParent mParent;
    @Mock
    private SuggestionsUiDelegate mUiDelegate;
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
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, mUiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        notifyTileUrlsAvailable();

        verify(mTileGroupObserver).onTileCountChanged();
        verify(mTileGroupObserver).onLoadTaskCompleted();
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testInitialiseWithOfflineUrl() {
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, mUiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        OfflineUrlsCallbackHandler callbackHandler = new OfflineUrlsCallbackHandler();
        doAnswer(callbackHandler)
                .when(mUiDelegate)
                .getUrlsAvailableOffline(ArgumentMatchers.<Set<String>>any(),
                        ArgumentMatchers.<Callback<Set<String>>>any());

        notifyTileUrlsAvailable(URLS);

        InOrder inOrder = inOrder(mTileGroupObserver);
        inOrder.verify(mTileGroupObserver).onTileCountChanged();
        inOrder.verify(mTileGroupObserver).onLoadTaskCompleted();
        inOrder.verify(mTileGroupObserver).onTileDataChanged();

        callbackHandler.engage(Collections.singleton(URLS[1]));

        // Notification about the url that should be marked as offline.
        inOrder.verify(mTileGroupObserver).onTileDataChanged();
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testInitialiseNoOfflineUrl() {
        TileGroup tileGroup = new TileGroup(RuntimeEnvironment.application, mUiDelegate,
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                TILE_TITLE_LINES);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        OfflineUrlsCallbackHandler callbackHandler = new OfflineUrlsCallbackHandler();
        doAnswer(callbackHandler)
                .when(mUiDelegate)
                .getUrlsAvailableOffline(ArgumentMatchers.<Set<String>>any(),
                        ArgumentMatchers.<Callback<Set<String>>>any());

        notifyTileUrlsAvailable(URLS);

        InOrder inOrder = inOrder(mTileGroupObserver);
        inOrder.verify(mTileGroupObserver).onTileCountChanged();
        inOrder.verify(mTileGroupObserver).onLoadTaskCompleted();
        inOrder.verify(mTileGroupObserver).onTileDataChanged();

        callbackHandler.engage(Collections.<String>emptySet());

        // Triggering the offline callback does not cause updates because the data has not changed.
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

    private static class OfflineUrlsCallbackHandler implements Answer<Void> {
        private Callback<Set<String>> mCallback;
        private Set<String> mCallbackArgument;

        @Override
        public Void answer(InvocationOnMock invocation) throws Throwable {
            mCallback = invocation.getArgument(1);
            if (mCallbackArgument != null) invokeCallback();
            return null;
        }

        public void engage(Set<String> callbackArgument) {
            mCallbackArgument = callbackArgument;
            if (mCallbackArgument != null && mCallback != null) invokeCallback();
        }

        private void invokeCallback() {
            mCallback.onResult(mCallbackArgument);
            mCallback = null;
            mCallbackArgument = null;
        }
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
