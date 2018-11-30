// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cached_image_fetcher;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.BitmapCache;

/**
 * Unit tests for InMemoryCachedImageFetcher.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InMemoryCachedImageFetcherTest {
    private static final String URL = "http://foo.bar";
    private static final int WIDTH_PX = 100;
    private static final int HEIGHT_PX = 200;
    private static final int DEFAULT_CACHE_SIZE = 100;

    private final Bitmap mBitmap =
            Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);

    private InMemoryCachedImageFetcher mInMemoryCachedImageFetcher;
    private BitmapCache mBitmapCache;
    private DiscardableReferencePool mReferencePool;

    @Mock
    private CachedImageFetcherImpl mCachedImageFetcherImpl;
    @Mock
    private Callback<Bitmap> mCallback;

    @Captor
    private ArgumentCaptor<Integer> mWidthCaptor;
    @Captor
    private ArgumentCaptor<Integer> mHeightCaptor;
    @Captor
    private ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mReferencePool = new DiscardableReferencePool();
        mBitmapCache = new BitmapCache(mReferencePool, DEFAULT_CACHE_SIZE);
    }

    @After
    public void tearDown() {
        mBitmapCache.destroy();
        mReferencePool.drain();
    }

    public void answerFetch(Bitmap bitmap, CachedImageFetcher cachedImageFetcher,
            boolean deleteBitmapCacheOnFetch) {
        mInMemoryCachedImageFetcher =
                spy(new InMemoryCachedImageFetcher(mBitmapCache, cachedImageFetcher));
        // clang-format off
        doAnswer((InvocationOnMock invocation) -> {
            if (deleteBitmapCacheOnFetch) {
                mInMemoryCachedImageFetcher.destroy();
                mReferencePool.drain();
            }

            mCallbackCaptor.getValue().onResult(bitmap);
            return null;
        }).when(mCachedImageFetcherImpl)
                .fetchImage(eq(URL), mWidthCaptor.capture(), mHeightCaptor.capture(),
                        mCallbackCaptor.capture());
        // clang-format on
    }

    @SuppressWarnings("unchecked")
    @Test
    @SmallTest
    public void testFetchImageCachesFirstCall() throws Exception {
        answerFetch(mBitmap, mCachedImageFetcherImpl, false);
        mInMemoryCachedImageFetcher.fetchImage(URL, WIDTH_PX, HEIGHT_PX, mCallback);
        verify(mCallback).onResult(eq(mBitmap));

        reset(mCallback);
        mInMemoryCachedImageFetcher.fetchImage(URL, WIDTH_PX, HEIGHT_PX, mCallback);
        verify(mCallback).onResult(eq(mBitmap));

        verify(mCachedImageFetcherImpl, /* Should only go to native the first time. */ times(1))
                .fetchImage(eq(URL), eq(WIDTH_PX), eq(HEIGHT_PX), any());
    }

    @Test
    @SmallTest
    public void testFetchImageReturnsNullWhenFetcherIsNull() throws Exception {
        answerFetch(mBitmap, null, false);
        doReturn(null)
                .when(mInMemoryCachedImageFetcher)
                .tryToGetBitmap(eq(URL), eq(WIDTH_PX), eq(HEIGHT_PX));

        mInMemoryCachedImageFetcher.fetchImage(URL, WIDTH_PX, HEIGHT_PX, mCallback);
        verify(mCallback).onResult(eq(null));

        verify(mCachedImageFetcherImpl, /* Shouldn't make the call at all. */ times(0))
                .fetchImage(eq(URL), eq(WIDTH_PX), eq(HEIGHT_PX), any());
    }

    @Test
    @SmallTest
    public void testFetchImageDoesNotCacheAfterDestroy() {
        try {
            answerFetch(mBitmap, mCachedImageFetcherImpl, true);

            // No exception should be thrown here when bitmap cache is null.
            mInMemoryCachedImageFetcher.fetchImage(URL, WIDTH_PX, HEIGHT_PX, (Bitmap bitmap) -> {});
        } catch (Exception e) {
            fail("Destroy called in the middle of execution shouldn't throw");
        }
    }
}