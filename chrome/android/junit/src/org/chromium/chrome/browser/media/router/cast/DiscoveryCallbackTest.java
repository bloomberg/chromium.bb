// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.anyListOf;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.atMost;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.support.v7.media.MediaRouter;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.media.router.ChromeMediaRouter;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.List;

/**
 * Robolectric tests of {@link DiscoveryCallback} class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DiscoveryCallbackTest {
    static final String TEST_SOURCE_URN_1 =
            "urn:x-org.chromium:media:source:https://cast.google.com/#__castAppId__=ABCD";
    static final String TEST_SOURCE_URN_2 =
            "urn:x-org.chromium:media:source:https://cast.google.com/#__castAppId__=DCBA";

    /*
     * Test that the callback is not empty when created.
     */
    @Test
    @Feature("MediaRouter")
    public void testNotEmptyOnCreation() {
        DiscoveryCallback callback = new DiscoveryCallback(
                TEST_SOURCE_URN_1,
                mock(ChromeMediaRouter.class));
        assertFalse(callback.isEmpty());
    }

    /**
     * Tests that the callback sends the current list of sinks back immediately upon adding a
     * source URN.
     */
    @Test
    @Feature("MediaRouter")
    public void testReturnCurrentSinksWhenNewSourceUrnAdded() {
        MediaRouter.RouteInfo route1 = TestUtils.createMockRouteInfo("RouteId1", "RouteName1");
        MediaSink sink1 = MediaSink.fromRoute(route1);

        List<MediaSink> onlySink1 = new ArrayList<MediaSink>();
        onlySink1.add(sink1);

        ChromeMediaRouter mockMediaRouter = mock(ChromeMediaRouter.class);
        DiscoveryCallback callback = new DiscoveryCallback(
                TEST_SOURCE_URN_1,
                mockMediaRouter);

        // On creation one URN is added and sinks are returned immediately.
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_1, new ArrayList<MediaSink>());

        // When a route is discovered, the sinks are returned too.
        callback.onRouteAdded(null, route1);
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_1, onlySink1);

        // If a new source URN added, the current sinks are returned again for the new source only.
        callback.addSourceUrn(TEST_SOURCE_URN_2);
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_2, onlySink1);
        verify(mockMediaRouter, atMost(1)).onSinksReceived(TEST_SOURCE_URN_1, onlySink1);
    }

    /**
     * Tests that {@link DiscoveryCallback#addSourceUrn} and
     * {@link DiscoveryCallback#removeSourceUrn} behave like a set.
     */
    @Test
    @Feature("MediaRouter")
    public void testAddRemoveSourceUrnIsASet() {
        DiscoveryCallback callback = new DiscoveryCallback(
                TEST_SOURCE_URN_1,
                mock(ChromeMediaRouter.class));

        // Adding the same source URN, only need to remove it once to become empty.
        callback.addSourceUrn(TEST_SOURCE_URN_1);
        callback.removeSourceUrn(TEST_SOURCE_URN_1);
        assertTrue(callback.isEmpty());

        // Removing a non-existing source URN doesn't change anything.
        callback.removeSourceUrn(TEST_SOURCE_URN_2);
        assertTrue(callback.isEmpty());

        // Adding two different source URNs requires removing two different URNs.
        callback.addSourceUrn(TEST_SOURCE_URN_1);
        callback.addSourceUrn(TEST_SOURCE_URN_2);
        assertFalse(callback.isEmpty());
        callback.removeSourceUrn(TEST_SOURCE_URN_2);
        assertFalse(callback.isEmpty());

        // Removing the same source URN doesn't change anything.
        callback.removeSourceUrn(TEST_SOURCE_URN_2);
        assertFalse(callback.isEmpty());
        callback.removeSourceUrn(TEST_SOURCE_URN_1);
        assertTrue(callback.isEmpty());
    }

    /**
     * Tests that {@link DiscoveryCallback#onRouteAdded} updates the ChromeMediaRouter with the
     * correct list of sinks.
     */
    @Test
    @Feature("MediaRouter")
    public void testOnRouteAddedNewRoute() {
        ChromeMediaRouter mockMediaRouter = mock(ChromeMediaRouter.class);

        MediaRouter.RouteInfo route1 = TestUtils.createMockRouteInfo("RouteId1", "RouteName1");
        MediaRouter.RouteInfo route2 = TestUtils.createMockRouteInfo("RouteId2", "RouteName2");
        MediaSink sink1 = MediaSink.fromRoute(route1);
        MediaSink sink2 = MediaSink.fromRoute(route2);

        List<MediaSink> noSinks = new ArrayList<MediaSink>();
        List<MediaSink> onlySink1 = new ArrayList<MediaSink>();
        onlySink1.add(sink1);
        List<MediaSink> onlySink2 = new ArrayList<MediaSink>();
        onlySink2.add(sink2);
        List<MediaSink> bothSinks = new ArrayList<MediaSink>();
        bothSinks.add(sink1);
        bothSinks.add(sink2);
        List<MediaSink> bothSinksReverse = new ArrayList<MediaSink>();
        bothSinksReverse.add(sink2);
        bothSinksReverse.add(sink1);

        DiscoveryCallback callback = new DiscoveryCallback(
                TEST_SOURCE_URN_1,
                mockMediaRouter);

        // No routes are reported.
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_1, noSinks);

        // One route discovered.
        callback.onRouteAdded(null, route1);
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_1, onlySink1);

        // Second route discovered, both routes reported.
        callback.onRouteAdded(null, route2);
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_1, bothSinks);

        // A new source URN added - report the routes only for this source URN.
        callback.addSourceUrn(TEST_SOURCE_URN_2);
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_2, bothSinks);
        verify(mockMediaRouter, atMost(1)).onSinksReceived(TEST_SOURCE_URN_1, bothSinks);

        // One route removed - report the updated list for both source URNs.
        callback.onRouteRemoved(null, route1);
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_1, onlySink2);
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_2, onlySink2);

        // The last route removed, an empty array is received.
        callback.onRouteRemoved(null, route2);
        verify(mockMediaRouter, times(2)).onSinksReceived(TEST_SOURCE_URN_1, noSinks);
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_2, noSinks);

        // Receiving updates about sinks only for one source URN from now on.
        callback.removeSourceUrn(TEST_SOURCE_URN_1);

        // Discover routes in the reverse order.
        callback.onRouteAdded(null, route2);
        verify(mockMediaRouter, times(2)).onSinksReceived(TEST_SOURCE_URN_2, onlySink2);
        callback.onRouteAdded(null, route1);
        verify(mockMediaRouter).onSinksReceived(TEST_SOURCE_URN_2, bothSinksReverse);
        verify(mockMediaRouter, atMost(5)).onSinksReceived(
                eq(TEST_SOURCE_URN_1), anyListOf(MediaSink.class));
    }
}
