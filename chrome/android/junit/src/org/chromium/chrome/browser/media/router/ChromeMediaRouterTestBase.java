// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;

import org.chromium.base.CommandLine;
import org.junit.Before;
import org.robolectric.shadows.ShadowLog;

/**
 * Robolectric test base class for ChromeMediaRouter.
 */
public class ChromeMediaRouterTestBase {
    protected static final String SOURCE_ID1 = new StringBuilder()
            .append("https://google.com/cast#")
            .append("__castAppId__=CCCCCCCC/")
            .append("__castClientId__=11111111111111111/")
            .append("__castAutoJoinPolicy__=origin_scoped/")
            .append("__castLaunchTimeout__=10000")
            .toString();
    protected static final String SOURCE_ID2 = new StringBuilder()
            .append("https://google.com/cast#")
            .append("__castAppId__=CCCCCCCC/")
            .append("__castClientId__=222222222222222222/")
            .append("__castAutoJoinPolicy__=origin_scoped/")
            .append("__castLaunchTimeout__=10000")
            .toString();
    protected static final String SINK_ID1 = new StringBuilder()
            .append("com.google.android.gms/")
            .append(".cast.media.MediaRouteProviderService:cccccccccccccccccccccccccccccccc")
            .toString();
    protected static final String SINK_ID2 = new StringBuilder()
            .append("com.google.android.gms/")
            .append(".cast.media.MediaRouteProviderService:dddddddddddddddddddddddddddddddd")
            .toString();
    protected static final String SINK_NAME1 = "sink name 1";
    protected static final String SINK_NAME2 = "sink name 2";
    protected static final String PRESENTATION_ID1 = "mr_CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC";
    protected static final String PRESENTATION_ID2 = "mr_DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD";
    protected static final String ORIGIN1 = "http://www.example1.com/";
    protected static final String ORIGIN2 = "http://www.example2.com/";
    protected static final int TAB_ID1 = 1;
    protected static final int TAB_ID2 = 2;
    protected static final int REQUEST_ID1 = 1;
    protected static final int REQUEST_ID2 = 2;
    protected ChromeMediaRouter mChromeMediaRouter;
    protected MediaRouteProvider mRouteProvider;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        mChromeMediaRouter = spy(new ChromeMediaRouter(0));
        mRouteProvider = mock(MediaRouteProvider.class);
        doReturn(true).when(mRouteProvider).supportsSource(anyString());
        mChromeMediaRouter.addMediaRouteProvider(mRouteProvider);
        assertEquals(mChromeMediaRouter.getRouteProvidersForTest().size(), 1);
        assertEquals(mRouteProvider, mChromeMediaRouter.getRouteProvidersForTest().get(0));
        assertNotNull(mRouteProvider);

        // Initialize the command line to avoid an assertion failure in SysUtils.
        CommandLine.init(new String[0]);
    }
}
