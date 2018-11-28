// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cached_image_fetcher;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.BitmapCache;
import org.chromium.chrome.browser.util.test.ShadowUrlUtilities;

/**
 * Unit tests for InMemoryCachedImageFetcher.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowUrlUtilities.class, BackgroundShadowAsyncTask.class})
public class InMemoryCachedImageFetcherTest {
    private static final String URL = "http://foo.bar";
    private static final int WIDTH_PX = 100;
    private static final int HEIGHT_PX = 200;
    private static final int DEFAULT_CACHE_SIZE = 100;

    private final DiscardableReferencePool mReferencePool = new DiscardableReferencePool();
    private final Bitmap mBitmap =
            Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);

    private InMemoryCachedImageFetcher mInMemoryCachedImageFetcher;
    private BitmapCache mBitmapCache;

    @Mock
    private CachedImageFetcherImpl mCachedImageFetcherImpl;

    @Captor
    private ArgumentCaptor<Integer> mWidthCaptor;
    @Captor
    private ArgumentCaptor<Integer> mHeightCaptor;
    @Captor
    private ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mBitmapCache = new BitmapCache(mReferencePool, DEFAULT_CACHE_SIZE);
        mInMemoryCachedImageFetcher =
                new InMemoryCachedImageFetcher(mBitmapCache, mCachedImageFetcherImpl);
        // clang-format off
        doAnswer((InvocationOnMock invocation) -> {
            mCallbackCaptor.getValue().onResult(mBitmap);
            return null;
        }).when(mCachedImageFetcherImpl)
          .fetchImage(eq(URL), mWidthCaptor.capture(), mHeightCaptor.capture(),
                      mCallbackCaptor.capture());
        // clang-format on
    }

    @Test
    @SmallTest
    public void testFetchImageCachesFirstCall() throws Exception {
        mInMemoryCachedImageFetcher.fetchImage(
                URL, WIDTH_PX, HEIGHT_PX, (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        mInMemoryCachedImageFetcher.fetchImage(
                URL, WIDTH_PX, HEIGHT_PX, (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        verify(mCachedImageFetcherImpl, /* Should only go to native the first time. */ times(1))
                .fetchImage(eq(URL), eq(WIDTH_PX), eq(HEIGHT_PX), any());
    }
}