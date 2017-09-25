// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.support.v7.media.MediaRouter;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.media.router.ChromeMediaRouter;
import org.chromium.chrome.browser.media.router.MediaRoute;
import org.chromium.chrome.browser.media.router.MediaRouteManager;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;

/**
 * Robolectric tests for {@link CastMediaRouteProvider}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastMediaRouteProviderTest {
    private static final String SUPPORTED_SOURCE = "cast:DEADBEEF";

    // TODO(crbug.com/672704): Android does not currently support 1-UA mode.
    private static final String UNSUPPORTED_SOURCE = "https://example.com";

    @Test
    @Feature({"MediaRouter"})
    public void testStartObservingMediaSinksNoMediaRouter() {
        ChromeMediaRouter.setAndroidMediaRouterForTest(null);

        MediaRouteManager mockManager = mock(MediaRouteManager.class);
        CastMediaRouteProvider provider = CastMediaRouteProvider.create(mockManager);

        provider.startObservingMediaSinks(SUPPORTED_SOURCE);

        verify(mockManager, timeout(100))
                .onSinksReceived(
                        eq(SUPPORTED_SOURCE), same(provider), eq(new ArrayList<MediaSink>()));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testStartObservingMediaSinksUnsupportedSource() {
        ChromeMediaRouter.setAndroidMediaRouterForTest(mock(MediaRouter.class));

        MediaRouteManager mockManager = mock(MediaRouteManager.class);
        CastMediaRouteProvider provider = CastMediaRouteProvider.create(mockManager);

        provider.startObservingMediaSinks(UNSUPPORTED_SOURCE);

        verify(mockManager, timeout(100))
                .onSinksReceived(
                        eq(UNSUPPORTED_SOURCE), same(provider), eq(new ArrayList<MediaSink>()));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testOnSessionClosedNoClientRecord() {
        ChromeMediaRouter.setAndroidMediaRouterForTest(mock(MediaRouter.class));

        MediaRouteManager mockManager = mock(MediaRouteManager.class);
        CastMediaRouteProvider provider = CastMediaRouteProvider.create(mockManager);

        CastSession mockSession = mock(CastSession.class);
        provider.onSessionCreated(mockSession);

        MediaRoute route = new MediaRoute("sink", SUPPORTED_SOURCE, "");
        provider.addRoute(route, "", -1);
        provider.onSessionClosed();

        verify(mockManager).onRouteClosed(route.id);
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCloseRouteWithNoSession() {
        ChromeMediaRouter.setAndroidMediaRouterForTest(mock(MediaRouter.class));

        MediaRouteManager mockManager = mock(MediaRouteManager.class);
        CastMediaRouteProvider provider = CastMediaRouteProvider.create(mockManager);

        MediaRoute route = new MediaRoute("sink", SUPPORTED_SOURCE, "");
        provider.addRoute(route, "", -1);
        provider.closeRoute(route.id);

        verify(mockManager).onRouteClosed(route.id);
    }
}
