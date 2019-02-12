// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.graphics.Bitmap;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.cached_image_fetcher.CachedImageFetcher;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorTest.ShadowUrlUtilities;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

/**
 * Unit tests for CachedImageFetcherImpl.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowUrlUtilities.class, BackgroundShadowAsyncTask.class})
public class ContextualSuggestionsSourceImplTest {
    private static final String IMAGE_URL = "http://foo.com/bar.png";
    private static final int FAVICON_DIMENSION = 100;

    ContextualSuggestionsSourceImpl mContextualSuggestionsSourceImpl;

    @Mock
    ContextualSuggestionsBridge mBridge;

    @Mock
    CachedImageFetcher mCachedImageFetcher;

    @Mock
    Bitmap mBitmap;

    @Captor
    ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mContextualSuggestionsSourceImpl =
                new ContextualSuggestionsSourceImpl(mBridge, mCachedImageFetcher);
        // Answer with or without width/height.
        doAnswer((InvocationOnMock invocation) -> {
            mCallbackCaptor.getValue().onResult(mBitmap);
            return null;
        })
                .when(mCachedImageFetcher)
                .fetchImage(any(), any(), mCallbackCaptor.capture());
    }

    @Test
    @SmallTest
    public void testFetchContextualSuggestionImage() throws Exception {
        doReturn(IMAGE_URL).when(mBridge).getImageUrl(any());

        SnippetArticle article = mock(SnippetArticle.class);
        mContextualSuggestionsSourceImpl.fetchContextualSuggestionImage(
                article, (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
    }

    @Test
    @SmallTest
    public void testFetchContextualSuggestionImageWhenNullUrlIsReturned() throws Exception {
        doReturn(null).when(mBridge).getImageUrl(any());

        SnippetArticle article = mock(SnippetArticle.class);
        mContextualSuggestionsSourceImpl.fetchContextualSuggestionImage(
                article, (Bitmap bitmap) -> { assertEquals(bitmap, null); });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
    }

    @Test
    @SmallTest
    public void testFetchContextualSuggestionFavicon() throws Exception {
        doReturn(IMAGE_URL).when(mBridge).getFaviconUrl(any());

        SnippetArticle article = mock(SnippetArticle.class);
        mContextualSuggestionsSourceImpl.fetchSuggestionFavicon(article, FAVICON_DIMENSION,
                FAVICON_DIMENSION, (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
    }

    @Test
    @SmallTest
    public void testFetchContextualSuggestionFaviconNullUrlIsReturned() throws Exception {
        doReturn(null).when(mBridge).getFaviconUrl(any());

        SnippetArticle article = mock(SnippetArticle.class);
        mContextualSuggestionsSourceImpl.fetchSuggestionFavicon(article, FAVICON_DIMENSION,
                FAVICON_DIMENSION, (Bitmap bitmap) -> { assertEquals(bitmap, null); });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
    }
}