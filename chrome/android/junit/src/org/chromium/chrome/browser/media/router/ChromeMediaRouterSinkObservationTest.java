// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.media.router.cast.MediaSink;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.List;

/**
 * Sink observation tests for ChromeMediaRouter.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = { ChromeMediaRouterTestBase.FakeActivityManager.class })
public class ChromeMediaRouterSinkObservationTest extends ChromeMediaRouterTestBase {
    @Test
    @Feature({"MediaRouter"})
    public void testRouterOnSinksReceived() {
        mChromeMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, new ArrayList<MediaSink>());

        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest().size(), 1);
        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                .get(SOURCE_ID1).size(), 1);
        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                .get(SOURCE_ID1).get(mRouteProvider).size(), 0);
        assertEquals(mChromeMediaRouter.getSinksPerSourceForTest().size(), 1);
        assertEquals(mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).size(), 0);
    }

    @Test
    @Feature({"MediaRouter"})
    public void testRouterOnSinksReceivedTwiceForOneSource() {
        mChromeMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, new ArrayList<MediaSink>());

        List<MediaSink> sinkList = new ArrayList<MediaSink>();
        MediaSink sink = new MediaSink(SINK_ID1, SINK_NAME1, null);
        sinkList.add(sink);
        mChromeMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, sinkList);

        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest().size(), 1);
        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                .get(SOURCE_ID1).size(), 1);
        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                .get(SOURCE_ID1).get(mRouteProvider).size(), 1);
        assertTrue(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                   .get(SOURCE_ID1).get(mRouteProvider).contains(sink));

        assertEquals(mChromeMediaRouter.getSinksPerSourceForTest().size(), 1);
        assertEquals(mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).size(), 1);
        assertTrue(mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).contains(sink));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testRouterOnSinksReceivedForTwoSources() {
        mChromeMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, new ArrayList<MediaSink>());

        List<MediaSink> sinkList = new ArrayList<MediaSink>();
        MediaSink sink = new MediaSink(SINK_ID1, SINK_NAME1, null);
        sinkList.add(sink);
        mChromeMediaRouter.onSinksReceived(SOURCE_ID2, mRouteProvider, sinkList);

        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest().size(), 2);
        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                .get(SOURCE_ID1).size(), 1);
        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                .get(SOURCE_ID1).get(mRouteProvider).size(), 0);
        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                .get(SOURCE_ID2).size(), 1);
        assertEquals(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                .get(SOURCE_ID2).get(mRouteProvider).size(), 1);
        assertTrue(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                   .get(SOURCE_ID2).get(mRouteProvider).contains(sink));
        assertEquals(mChromeMediaRouter.getSinksPerSourceForTest().size(), 2);
        assertEquals(mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).size(), 0);
        assertEquals(mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID2).size(), 1);
        assertTrue(mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID2).contains(sink));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testNotLowRamDevice() {
        sIsLowRamDevice = false;
        assertTrue(mChromeMediaRouter.startObservingMediaSinks(SOURCE_ID1));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testIsLowRamDevice() {
        sIsLowRamDevice = true;
        assertEquals(Build.VERSION.SDK_INT <= Build.VERSION_CODES.JELLY_BEAN_MR2,
                     mChromeMediaRouter.startObservingMediaSinks(SOURCE_ID1));
    }
}
