// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwWebContentsObserver;
import org.chromium.base.test.util.Feature;
import org.chromium.net.NetError;

/**
 * Tests for the AwWebContentsObserver class.
 */
public class AwWebContentsObserverTest extends AwTestBase  {
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwWebContentsObserver mWebContentsObserver;

    private static final String EXAMPLE_URL = "http://www.example.com/";
    private static final String ERROR_DESCRIPTION = "description";
    private String mUnreachableWebDataUrl;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mUnreachableWebDataUrl = AwContentsStatics.getUnreachableWebDataUrl();
        // AwWebContentsObserver constructor must be run on the UI thread.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mWebContentsObserver = new AwWebContentsObserver(
                        mTestContainerView.getContentViewCore().getWebContents(), mContentsClient);
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinished() {
        int frameId = 0;
        boolean mainFrame = true;
        boolean subFrame = false;

        int callCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, EXAMPLE_URL, subFrame);
        assertEquals("onPageFinished should only be called for the main frame.", callCount,
                mContentsClient.getOnPageFinishedHelper().getCallCount());

        callCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, mUnreachableWebDataUrl, mainFrame);
        assertEquals("onPageFinished should not be called for the error url.", callCount,
                mContentsClient.getOnPageFinishedHelper().getCallCount());

        callCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, EXAMPLE_URL, mainFrame);
        assertEquals("onPageFinished should be called for main frame navigations.", callCount + 1,
                mContentsClient.getOnPageFinishedHelper().getCallCount());

        boolean provisionalLoad = true;

        callCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mWebContentsObserver.didFailLoad(!provisionalLoad, mainFrame,
                NetError.ERR_ABORTED, ERROR_DESCRIPTION, EXAMPLE_URL);
        assertEquals("onPageFinished should be called for main frame errors.", callCount + 1,
                mContentsClient.getOnPageFinishedHelper().getCallCount());

        callCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mWebContentsObserver.didFailLoad(!provisionalLoad, subFrame,
                NetError.ERR_ABORTED, ERROR_DESCRIPTION, EXAMPLE_URL);
        assertEquals("onPageFinished should only be called for main frame errors.", callCount,
                mContentsClient.getOnPageFinishedHelper().getCallCount());

        callCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mWebContentsObserver.didFailLoad(!provisionalLoad, mainFrame,
                NetError.ERR_ABORTED, ERROR_DESCRIPTION, mUnreachableWebDataUrl);
        assertEquals("onPageFinished should not be called on unrechable url errors.", callCount,
                mContentsClient.getOnPageFinishedHelper().getCallCount());

        String baseUrl = null;
        boolean navigationToDifferentPage = true;
        boolean fragmentNavigation = true;
        callCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mWebContentsObserver.didNavigateMainFrame(EXAMPLE_URL, baseUrl,
                !navigationToDifferentPage, fragmentNavigation);
        assertEquals("onPageFinished should be called for main frame fragment navigations.",
                callCount + 1, mContentsClient.getOnPageFinishedHelper().getCallCount());

        callCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mWebContentsObserver.didNavigateMainFrame(EXAMPLE_URL, baseUrl,
                !navigationToDifferentPage, !fragmentNavigation);
        assertEquals("onPageFinished should be called only for main frame fragment navigations.",
                callCount, mContentsClient.getOnPageFinishedHelper().getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedError() {
        boolean provisionalLoad = true;
        boolean mainFrame = true;
        boolean subFrame = false;

        int callCount = mContentsClient.getOnReceivedErrorHelper().getCallCount();
        mWebContentsObserver.didFailLoad(!provisionalLoad, subFrame,
                NetError.ERR_TIMED_OUT, ERROR_DESCRIPTION, EXAMPLE_URL);
        assertEquals("onReceivedError should only be called for the main frame", callCount,
                mContentsClient.getOnReceivedErrorHelper().getCallCount());

        callCount = mContentsClient.getOnReceivedErrorHelper().getCallCount();
        mWebContentsObserver.didFailLoad(!provisionalLoad, mainFrame,
                NetError.ERR_TIMED_OUT, ERROR_DESCRIPTION, EXAMPLE_URL);
        assertEquals("onReceivedError should be called for the main frame", callCount + 1,
                mContentsClient.getOnReceivedErrorHelper().getCallCount());

        callCount = mContentsClient.getOnReceivedErrorHelper().getCallCount();
        mWebContentsObserver.didFailLoad(!provisionalLoad, mainFrame,
                NetError.ERR_ABORTED, ERROR_DESCRIPTION, EXAMPLE_URL);
        assertEquals("onReceivedError should not be called for aborted navigations", callCount,
                mContentsClient.getOnReceivedErrorHelper().getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDidNavigateMainFrame() {
        String nullUrl = null;
        String baseUrl = null;
        boolean reload = true;

        int callCount = mContentsClient.getDoUpdateVisitedHistoryHelper().getCallCount();
        mWebContentsObserver.didNavigateAnyFrame(nullUrl, baseUrl, !reload);
        assertEquals("doUpdateVisitedHistory should only be called for any url.", callCount + 1,
                mContentsClient.getDoUpdateVisitedHistoryHelper().getCallCount());
        assertEquals(!reload, mContentsClient.getDoUpdateVisitedHistoryHelper().getIsReload());

        callCount = mContentsClient.getDoUpdateVisitedHistoryHelper().getCallCount();
        mWebContentsObserver.didNavigateAnyFrame(EXAMPLE_URL, baseUrl, !reload);
        assertEquals("doUpdateVisitedHistory should only be called for any url.", callCount + 1,
                mContentsClient.getDoUpdateVisitedHistoryHelper().getCallCount());
        assertEquals(!reload, mContentsClient.getDoUpdateVisitedHistoryHelper().getIsReload());

        callCount = mContentsClient.getDoUpdateVisitedHistoryHelper().getCallCount();
        mWebContentsObserver.didNavigateAnyFrame(EXAMPLE_URL, baseUrl, reload);
        assertEquals("doUpdateVisitedHistory should be called for reloads.", callCount + 1,
                mContentsClient.getDoUpdateVisitedHistoryHelper().getCallCount());
        assertEquals(reload, mContentsClient.getDoUpdateVisitedHistoryHelper().getIsReload());
    }
}
