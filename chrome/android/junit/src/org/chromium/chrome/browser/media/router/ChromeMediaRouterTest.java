// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyListOf;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.atMost;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;

import android.content.Context;
import android.support.v7.media.MediaRouteSelector;
import android.support.v7.media.MediaRouter;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.media.router.cast.DiscoveryCallback;
import org.chromium.chrome.browser.media.router.cast.MediaSink;
import org.chromium.chrome.browser.media.router.cast.MediaSource;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/**
 * Robolectric tests of ChromeMediaRouter class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ChromeMediaRouterTest.ShadowMediaRouter.class)
public class ChromeMediaRouterTest {
    private static final long NATIVE_MEDIA_ROUTER = -1;

    /** Reset the environment before each test. */
    @Before
    public void beforeTest() {
        ShadowMediaRouter.reset();
    }

    /**
     * Test method for {@link ChromeMediaRouter#startObservingMediaSinks}.
     */
    @Test
    @Feature("MediaRouter")
    public void testStartObservingMediaSinks() {
        MediaRouter mockAndroidMediaRouter = mock(MediaRouter.class);
        ShadowMediaRouter.sMediaRouter = mockAndroidMediaRouter;

        // Source URN that's not applicable to this route provider.
        ChromeMediaRouter router = spy(new ChromeMediaRouter(
                NATIVE_MEDIA_ROUTER, mock(Context.class)));
        doNothing().when(router).onSinksReceived(anyString(), anyListOf(MediaSink.class));

        router.startObservingMediaSinks("irrelevantsourceuri");
        verifyZeroInteractions(mockAndroidMediaRouter);

        // A valid source URN. A callback must be added with the right app id.
        router.startObservingMediaSinks("https://google.com/cast#__castAppId__=DEADBEEF");
        verify(mockAndroidMediaRouter).addCallback(
                eq(buildRouteSelector("DEADBEEF")),
                any(DiscoveryCallback.class),
                eq(MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY));

        // Another source URN with the same Cast application id. No callbacks should be added.
        router.startObservingMediaSinks(
                "https://google.com/cast#__castAppId__=DEADBEEF/__castAutoJoin__=origin");
        verify(mockAndroidMediaRouter, atMost(1)).addCallback(
                any(MediaRouteSelector.class), any(DiscoveryCallback.class), anyInt());

        // Another source URN with a different Cast application id.
        router.startObservingMediaSinks("https://google.com/cast#__castAppId__=BEEFDEAD");
        verify(mockAndroidMediaRouter).addCallback(
                eq(buildRouteSelector("DEADBEEF")),
                any(DiscoveryCallback.class),
                eq(MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY));
    }

    /**
     * Test method for {@link ChromeMediaRouter#stopObservingMediaSinks}.
     */
    @Test
    @Feature("MediaRouter")
    public void testStopObservingMediaSinks() {
        MediaRouter mockAndroidMediaRouter = mock(MediaRouter.class);
        ShadowMediaRouter.sMediaRouter = mockAndroidMediaRouter;

        // Two source URNs for the same application id and one more with a different application id.
        // Two callbacks added, one has two source URNs.
        ChromeMediaRouter router = spy(new ChromeMediaRouter(
                NATIVE_MEDIA_ROUTER, mock(Context.class)));
        doNothing().when(router).onSinksReceived(anyString(), anyListOf(MediaSink.class));

        router.startObservingMediaSinks("https://google.com/cast#__castAppId__=DEADBEEF");
        ArgumentCaptor<MediaRouter.Callback> callback1 =
                ArgumentCaptor.forClass(MediaRouter.Callback.class);
        verify(mockAndroidMediaRouter).addCallback(
                eq(buildRouteSelector("DEADBEEF")),
                callback1.capture(),
                eq(MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY));
        router.startObservingMediaSinks(
                "https://google.com/cast#__castAppId__=DEADBEEF/__castAutoJoin__=origin");
        router.startObservingMediaSinks("https://google.com/cast#__castAppId__=BEEFDEAD");
        ArgumentCaptor<MediaRouter.Callback> callback2 =
                ArgumentCaptor.forClass(MediaRouter.Callback.class);
        verify(mockAndroidMediaRouter).addCallback(
                eq(buildRouteSelector("BEEFDEAD")),
                callback2.capture(),
                eq(MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY));

        // Nobody observes this application id anymore.
        router.stopObservingMediaSinks("https://google.com/cast#__castAppId__=BEEFDEAD");
        verify(mockAndroidMediaRouter).removeCallback(callback2.getValue());

        // This application id is still observed via another source URN.
        router.stopObservingMediaSinks("https://google.com/cast#__castAppId__=DEADBEEF");

        // Unregistered source URNs are also ignored.
        router.stopObservingMediaSinks("https://google.com/cast#__castAppId__=DEADBEEF/garbage");
        verify(mockAndroidMediaRouter, atMost(1)).removeCallback(any(MediaRouter.Callback.class));

        // Finally the first callback is removed.
        router.stopObservingMediaSinks(
                "https://google.com/cast#__castAppId__=DEADBEEF/__castAutoJoin__=origin");
        verify(mockAndroidMediaRouter, atMost(1)).removeCallback(callback1.getValue());
        verify(mockAndroidMediaRouter, atMost(2)).removeCallback(any(MediaRouter.Callback.class));
    }

    /** Shadow needed because getInstance() can't be mocked */
    @Implements(MediaRouter.class)
    public static class ShadowMediaRouter {
        public static MediaRouter sMediaRouter;

        @Implementation
        public static MediaRouter getInstance(Context context) {
            return sMediaRouter;
        }

        public static void reset() {
            sMediaRouter = null;
        }
    }

    private MediaRouteSelector buildRouteSelector(String applicationId) {
        return new MediaSource(applicationId).buildRouteSelector();
    }
}