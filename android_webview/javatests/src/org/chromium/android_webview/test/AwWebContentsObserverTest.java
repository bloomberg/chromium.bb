// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwWebContentsObserver;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;

/**
 * Tests for the AwWebContentsObserver class.
 */
public class AwWebContentsObserverTest extends AwTestBase  {
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwWebContentsObserver mWebContentsObserver;

    private static final String EXAMPLE_URL = "http://www.example.com/";
    private static final String SYNC_URL = "http://example.org/";
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
                        mTestContainerView.getContentViewCore().getWebContents(),
                        mTestContainerView.getAwContents(), mContentsClient);
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinished() throws Throwable {
        int frameId = 0;
        boolean mainFrame = true;
        boolean subFrame = false;
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        int callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, EXAMPLE_URL, mainFrame);
        onPageFinishedHelper.waitForCallback(callCount);
        assertEquals("onPageFinished should be called for main frame navigations.", callCount + 1,
                onPageFinishedHelper.getCallCount());
        assertEquals("onPageFinished should be called for main frame navigations.", EXAMPLE_URL,
                onPageFinishedHelper.getUrl());

        // In order to check that callbacks are *not* firing, first we execute code
        // that shoudn't emit callbacks, then code that emits a callback, and check that we
        // have got only one callback, and that its URL is from the last call. Since
        // callbacks are serialized, that means we didn't have a callback for the first call.
        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, EXAMPLE_URL, subFrame);
        mWebContentsObserver.didFinishLoad(frameId, SYNC_URL, mainFrame);
        onPageFinishedHelper.waitForCallback(callCount);
        assertEquals("onPageFinished should only be called for the main frame.", callCount + 1,
                onPageFinishedHelper.getCallCount());
        assertEquals("onPageFinished should only be called for the main frame.", SYNC_URL,
                onPageFinishedHelper.getUrl());

        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, mUnreachableWebDataUrl, mainFrame);
        mWebContentsObserver.didFinishLoad(frameId, SYNC_URL, mainFrame);
        onPageFinishedHelper.waitForCallback(callCount);
        assertEquals("onPageFinished should not be called for the error url.", callCount + 1,
                onPageFinishedHelper.getCallCount());
        assertEquals("onPageFinished should not be called for the error url.", SYNC_URL,
                onPageFinishedHelper.getUrl());

        String baseUrl = null;
        boolean navigationToDifferentPage = true;
        boolean fragmentNavigation = true;
        int httpStatusCode = 200;
        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didNavigateMainFrame(EXAMPLE_URL, baseUrl,
                !navigationToDifferentPage, fragmentNavigation, httpStatusCode);
        onPageFinishedHelper.waitForCallback(callCount);
        assertEquals("onPageFinished should be called for main frame fragment navigations.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        assertEquals("onPageFinished should be called for main frame fragment navigations.",
                EXAMPLE_URL, onPageFinishedHelper.getUrl());

        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didNavigateMainFrame(EXAMPLE_URL, baseUrl,
                !navigationToDifferentPage, !fragmentNavigation, httpStatusCode);
        mWebContentsObserver.didFinishLoad(frameId, SYNC_URL, mainFrame);
        onPageFinishedHelper.waitForCallback(callCount);
        onPageFinishedHelper.waitForCallback(callCount);
        assertEquals("onPageFinished should be called only for main frame fragment navigations.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        assertEquals("onPageFinished should be called only for main frame fragment navigations.",
                SYNC_URL, onPageFinishedHelper.getUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDidNavigateMainFrame() throws Throwable {
        String nullUrl = null;
        String baseUrl = null;
        boolean reload = true;
        TestAwContentsClient.DoUpdateVisitedHistoryHelper doUpdateVisitedHistoryHelper =
                mContentsClient.getDoUpdateVisitedHistoryHelper();

        int callCount = doUpdateVisitedHistoryHelper.getCallCount();
        mWebContentsObserver.didNavigateAnyFrame(nullUrl, baseUrl, !reload);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        assertEquals("doUpdateVisitedHistory should be called for any url.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        assertEquals("doUpdateVisitedHistory should be called for any url.", nullUrl,
                doUpdateVisitedHistoryHelper.getUrl());
        assertEquals(!reload, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        mWebContentsObserver.didNavigateAnyFrame(EXAMPLE_URL, baseUrl, !reload);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        assertEquals("doUpdateVisitedHistory should be called for any url.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        assertEquals("doUpdateVisitedHistory should be called for any url.", EXAMPLE_URL,
                doUpdateVisitedHistoryHelper.getUrl());
        assertEquals(!reload, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        mWebContentsObserver.didNavigateAnyFrame(EXAMPLE_URL, baseUrl, reload);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        assertEquals("doUpdateVisitedHistory should be called for reloads.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        assertEquals("doUpdateVisitedHistory should be called for reloads.", EXAMPLE_URL,
                doUpdateVisitedHistoryHelper.getUrl());
        assertEquals(reload, doUpdateVisitedHistoryHelper.getIsReload());
    }
}
