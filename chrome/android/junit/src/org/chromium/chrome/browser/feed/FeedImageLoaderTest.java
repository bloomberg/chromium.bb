// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.common.functional.Consumer;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.AdditionalMatchers;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feed.FeedImageLoaderBridge.ImageResponse;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link FeedImageLoader}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedImageLoaderTest {
    public static final String HTTP_STRING1 = "http://www.test1.com";
    public static final String HTTP_STRING2 = "http://www.test2.com";
    public static final String HTTP_STRING3 = "http://www.test3.com";
    public static final String ASSET_STRING = "asset://logo_avatar_anonymous";
    public static final String OVERLAY_IMAGE_START =
            "overlay-image://?direction=start&url=https://www.test1.com";
    public static final String OVERLAY_IMAGE_END =
            "overlay-image://?direction=end&url=https://www.test1.com";
    public static final String OVERLAY_IMAGE_MISSING_URL = "overlay-image://?direction=end";
    public static final String OVERLAY_IMAGE_MISSING_DIRECTION =
            "overlay-image://?url=https://www.test1.com";
    public static final String OVERLAY_IMAGE_BAD_DIRECTION =
            "overlay-image://?direction=east&url=https://www.test1.com";

    @Mock
    private FeedImageLoaderBridge mBridge;
    @Mock
    private Consumer<Drawable> mConsumer;
    @Mock
    private Profile mProfile;
    @Captor
    ArgumentCaptor<List<String>> mUrlListArgument;
    @Captor
    ArgumentCaptor<Callback<ImageResponse>> mCallbackArgument;

    private FeedImageLoader mImageLoader;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        doNothing().when(mBridge).init(eq(mProfile));
        mImageLoader = new FeedImageLoader(mProfile, ContextUtils.getApplicationContext(), mBridge);
        verify(mBridge, times(1)).init(eq(mProfile));
        answerFetchImage(-1, null);
    }

    private void answerFetchImage(int imagePositionInList, Bitmap bitmap) {
        Answer<Void> answer = new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mCallbackArgument.getValue().onResult(
                    new ImageResponse(imagePositionInList, bitmap));

                return null;
            }
        };
        doAnswer(answer).when(mBridge).fetchImage(
                mUrlListArgument.capture(), mCallbackArgument.capture());
    }

    @Test
    @SmallTest
    public void downloadImageTest() {
        List<String> urls = Arrays.asList(HTTP_STRING1);
        mImageLoader.loadDrawable(urls, mConsumer);

        verify(mBridge, times(1))
                .fetchImage(mUrlListArgument.capture(), mCallbackArgument.capture());
    }

    @Test
    @SmallTest
    public void onlyNetworkURLSendToBridgeTest() {
        List<String> urls = Arrays.asList(HTTP_STRING1, HTTP_STRING2, ASSET_STRING, HTTP_STRING3);
        mImageLoader.loadDrawable(urls, mConsumer);
        List<String> expected_urls = Arrays.asList(HTTP_STRING1, HTTP_STRING2, HTTP_STRING3);

        verify(mBridge, times(1)).fetchImage(eq(expected_urls), mCallbackArgument.capture());
    }

    @Test
    @SmallTest
    public void assetImageTest() {
        List<String> urls = Arrays.asList(ASSET_STRING);
        mImageLoader.loadDrawable(urls, mConsumer);

        verify(mConsumer, times(1)).accept(AdditionalMatchers.not(eq(null)));
    }

    @Test
    @SmallTest
    public void sendNullIfDownloadFailTest() {
        List<String> urls = Arrays.asList(HTTP_STRING1, HTTP_STRING2, HTTP_STRING3);
        mImageLoader.loadDrawable(urls, mConsumer);

        verify(mConsumer, times(1)).accept(eq(null));
    }

    @Test
    @SmallTest
    public void nullUrlListTest() {
        List<String> urls = Arrays.asList();
        mImageLoader.loadDrawable(urls, mConsumer);

        verify(mConsumer, times(1)).accept(eq(null));
    }

    @Test
    @SmallTest
    public void overlayImageTest_Start() {
        List<String> urls = Arrays.asList(OVERLAY_IMAGE_START);
        mImageLoader.loadDrawable(urls, mConsumer);

        verify(mBridge, times(1))
                .fetchImage(mUrlListArgument.capture(), mCallbackArgument.capture());
    }

    @Test
    @SmallTest
    public void overlayImageTest_End() {
        List<String> urls = Arrays.asList(OVERLAY_IMAGE_END);
        mImageLoader.loadDrawable(urls, mConsumer);

        verify(mBridge, times(1))
                .fetchImage(mUrlListArgument.capture(), mCallbackArgument.capture());
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void overlayImageTest_MissingUrl() {
        List<String> urls = Arrays.asList(OVERLAY_IMAGE_MISSING_URL);
        mImageLoader.loadDrawable(urls, mConsumer);

        verify(mConsumer, times(1)).accept(eq(null));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void overlayImageTest_MissingDirection() {
        List<String> urls = Arrays.asList(OVERLAY_IMAGE_MISSING_DIRECTION);
        mImageLoader.loadDrawable(urls, mConsumer);

        verify(mBridge, times(1))
                .fetchImage(mUrlListArgument.capture(), mCallbackArgument.capture());
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void overlayImageTest_BadDirection() {
        List<String> urls = Arrays.asList(OVERLAY_IMAGE_BAD_DIRECTION);
        mImageLoader.loadDrawable(urls, mConsumer);

        verify(mBridge, times(1))
                .fetchImage(mUrlListArgument.capture(), mCallbackArgument.capture());
    }
}
