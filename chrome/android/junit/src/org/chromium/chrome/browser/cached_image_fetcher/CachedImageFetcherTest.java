// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cached_image_fetcher;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorTest.ShadowUrlUtilities;

/**
 * Unit tests for CachedImageFetcher.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowUrlUtilities.class, CustomShadowAsyncTask.class})
public class CachedImageFetcherTest {
    private static String sDummyUrl = "foo.bar";

    CachedImageFetcher mCachedImageFetcher;

    @Mock
    CachedImageFetcherBridge mCachedImageFetcherBridge;

    @Mock
    Bitmap mBitmap;

    @Captor
    ArgumentCaptor<Callback<Bitmap>> mCallbackArgument;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mCachedImageFetcher = Mockito.spy(new CachedImageFetcher(mCachedImageFetcherBridge));
        Mockito.doReturn(sDummyUrl).when(mCachedImageFetcherBridge).getFilePath(anyObject());
    }

    private void answerCallback() {
        Mockito.doAnswer((InvocationOnMock invocation) -> {
                   mCallbackArgument.getValue().onResult(null);
                   return null;
               })
                .when(mCachedImageFetcherBridge)
                .fetchImage(anyObject(), anyInt(), anyInt(), mCallbackArgument.capture());
    }

    @Test
    @SmallTest
    public void testFetchImageWithDimensionsFoundOnDisk() {
        answerCallback();
        Mockito.doReturn(mBitmap)
                .when(mCachedImageFetcher)
                .tryToLoadImageFromDisk(anyObject(), anyObject());
        mCachedImageFetcher.fetchImage(sDummyUrl, 100, 100,
                (Bitmap bitmap)
                        -> {
                                // Do nothing.
                        });
        ArgumentCaptor<BitmapFactory.Options> optionsCaptor =
                ArgumentCaptor.forClass(BitmapFactory.Options.class);
        verify(mCachedImageFetcher).fetchImage(eq(sDummyUrl), optionsCaptor.capture(), any());

        BitmapFactory.Options opts = optionsCaptor.getValue();
        assertEquals(opts.outWidth, 100);
        assertEquals(opts.outHeight, 100);

        verify(mCachedImageFetcherBridge, never())
                .fetchImage(eq(sDummyUrl), anyInt(), anyInt(), any());
    }

    @Test
    @SmallTest
    public void testFetchImageWithNoDimensionsFoundOnDisk() {
        answerCallback();
        Mockito.doReturn(mBitmap)
                .when(mCachedImageFetcher)
                .tryToLoadImageFromDisk(anyObject(), anyObject());
        mCachedImageFetcher.fetchImage(sDummyUrl,
                (Bitmap bitmap)
                        -> {
                                // Do nothing.
                        });
        ArgumentCaptor<BitmapFactory.Options> optionsCaptor =
                ArgumentCaptor.forClass(BitmapFactory.Options.class);
        verify(mCachedImageFetcher).fetchImage(eq(sDummyUrl), optionsCaptor.capture(), any());

        BitmapFactory.Options opts = optionsCaptor.getValue();
        assertEquals(opts.outWidth, 0);
        assertEquals(opts.outHeight, 0);

        verify(mCachedImageFetcherBridge, never())
                .fetchImage(eq(sDummyUrl), anyInt(), anyInt(), any());
    }

    @Test
    @SmallTest
    public void testFetchImageWithNoDimensionsCallToNative() {
        answerCallback();
        Mockito.doReturn(null)
                .when(mCachedImageFetcher)
                .tryToLoadImageFromDisk(anyObject(), anyObject());
        mCachedImageFetcher.fetchImage(sDummyUrl,
                (Bitmap bitmap)
                        -> {
                                // Do nothing.
                        });
        ArgumentCaptor<BitmapFactory.Options> optionsCaptor =
                ArgumentCaptor.forClass(BitmapFactory.Options.class);
        verify(mCachedImageFetcher).fetchImage(eq(sDummyUrl), optionsCaptor.capture(), any());

        BitmapFactory.Options opts = optionsCaptor.getValue();
        assertEquals(opts.outWidth, 0);
        assertEquals(opts.outHeight, 0);

        verify(mCachedImageFetcherBridge).fetchImage(eq(sDummyUrl), anyInt(), anyInt(), any());
    }
}