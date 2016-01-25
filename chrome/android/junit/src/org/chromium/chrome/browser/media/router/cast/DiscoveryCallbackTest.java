// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import static org.chromium.chrome.browser.media.router.cast.TestUtils.createMockRouteInfo;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.media.router.ChromeMediaRouterTestBase;
import org.chromium.chrome.browser.media.router.DiscoveryDelegate;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.List;

/**
 * Robolectric tests for DiscoveryCallback.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = { ChromeMediaRouterTestBase.FakeActivityManager.class })
public class DiscoveryCallbackTest extends ChromeMediaRouterTestBase {
    protected DiscoveryDelegate mDiscoveryDelegate;

    @Before
    public void setUp() {
        super.setUp();
        mDiscoveryDelegate = mock(DiscoveryDelegate.class);
        assertNotNull(mDiscoveryDelegate);
    }

    @Test
    @Feature({"MediaRouter"})
    public void testInitCallbackWithEmptyKnownSinks() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(knownSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testInitCallbackWithNonemptyKnownSinks() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        MediaSink sink = new MediaSink(SINK_ID1, SINK_NAME1, null);
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(knownSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackAddOneSink() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackAddTwoSinks() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID2, SINK_NAME2));
        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        expectedSinks.add(new MediaSink(SINK_ID2, SINK_NAME2, null));
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackAddDuplicateSink() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        // Only expect one time. The duplicate add will not be notified.
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackRemoveSink() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.onRouteRemoved(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        // One time for init, one time for remove.
        verify(mDiscoveryDelegate, times(2)).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackRemoveNonexistingSink() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.onRouteRemoved(null, createMockRouteInfo(SINK_ID2, SINK_NAME2));

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        // Only one time for init.
        verify(mDiscoveryDelegate, times(1)).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackAddSourceUrn() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.addSourceUrn(SOURCE_ID2);

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID2), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackAddDuplicateSourceUrn() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.addSourceUrn(SOURCE_ID1);

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        // Only the one time after onRouteAdded().
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackRemoveSourceUrn() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.removeSourceUrn(SOURCE_ID1);

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        // Only the one time for init.
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackRemoveNonexistingSourceUrn() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback = new DiscoveryCallback(
                SOURCE_ID1, knownSinks, mDiscoveryDelegate);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.removeSourceUrn(SOURCE_ID2);

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        // Only the one time for init.
        verify(mDiscoveryDelegate, never()).onSinksReceived(eq(SOURCE_ID2), eq(expectedSinks));
    }
}
