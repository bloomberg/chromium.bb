// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;
import android.webkit.ValueCallback;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestAwContentsClient.DoUpdateVisitedHistoryHelper;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests for AwContentsClient.getVisitedHistory and AwContents.doUpdateVisitedHistory callbacks.
 */
public class AwContentsClientVisitedHistoryTest extends AwTestBase {
    private static class GetVisitedHistoryHelper extends CallbackHelper {
        private ValueCallback<String[]> mCallback;
        private boolean mSaveCallback;

        public ValueCallback<String[]> getCallback() {
            assert getCallCount() > 0;
            return mCallback;
        }

        public void setSaveCallback(boolean value) {
            mSaveCallback = value;
        }

        public void notifyCalled(ValueCallback<String[]> callback) {
            if (mSaveCallback) {
                mCallback = callback;
            }
            notifyCalled();
        }
    }

    private static class VisitedHistoryTestAwContentsClient extends TestAwContentsClient {

        private GetVisitedHistoryHelper mGetVisitedHistoryHelper;

        public VisitedHistoryTestAwContentsClient() {
            mGetVisitedHistoryHelper = new GetVisitedHistoryHelper();
        }

        public GetVisitedHistoryHelper getGetVisitedHistoryHelper() {
            return mGetVisitedHistoryHelper;
        }

        @Override
        public void getVisitedHistory(ValueCallback<String[]> callback) {
            getGetVisitedHistoryHelper().notifyCalled(callback);
        }

    }

    private VisitedHistoryTestAwContentsClient mContentsClient =
            new VisitedHistoryTestAwContentsClient();

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testUpdateVisitedHistoryCallback() throws Throwable {
        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        // Load a page with an iframe to make sure the callback only happens for the main frame URL.
        final String path = "/testUpdateVisitedHistoryCallback.html";
        final String html = "<iframe src=\"about:blank\"></iframe>";

        TestWebServer webServer = TestWebServer.start();
        try {
            final String pageUrl = webServer.setResponse(path, html, null);
            final DoUpdateVisitedHistoryHelper doUpdateVisitedHistoryHelper =
                    mContentsClient.getDoUpdateVisitedHistoryHelper();
            int callCount = doUpdateVisitedHistoryHelper.getCallCount();
            loadUrlAsync(awContents, pageUrl);
            doUpdateVisitedHistoryHelper.waitForCallback(callCount);
            assertEquals(pageUrl, doUpdateVisitedHistoryHelper.getUrl());
            assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());
            assertEquals(callCount + 1, doUpdateVisitedHistoryHelper.getCallCount());

            // Reload
            loadUrlAsync(awContents, pageUrl);
            doUpdateVisitedHistoryHelper.waitForCallback(callCount + 1);
            assertEquals(pageUrl, doUpdateVisitedHistoryHelper.getUrl());
            assertEquals(true, doUpdateVisitedHistoryHelper.getIsReload());
            assertEquals(callCount + 2, doUpdateVisitedHistoryHelper.getCallCount());
        } finally {
            webServer.shutdown();
        }
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGetVisitedHistoryExerciseCodePath() throws Throwable {
        // Due to security/privacy restrictions around the :visited css property, it is not
        // possible test this end to end without using the flaky and brittle capturing picture of
        // the web page. So we are doing the next best thing, exercising all the code paths.
        final GetVisitedHistoryHelper visitedHistoryHelper =
                mContentsClient.getGetVisitedHistoryHelper();
        final int callCount = visitedHistoryHelper.getCallCount();
        visitedHistoryHelper.setSaveCallback(true);

        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        final String path = "/testGetVisitedHistoryExerciseCodePath.html";
        final String visitedLinks[] = {"http://foo.com", "http://bar.com", null};
        final String html = "<a src=\"http://foo.com\">foo</a><a src=\"http://bar.com\">bar</a>";

        TestWebServer webServer = TestWebServer.start();
        try {
            final String pageUrl = webServer.setResponse(path, html, null);
            loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
            visitedHistoryHelper.waitForCallback(callCount);
            assertNotNull(visitedHistoryHelper.getCallback());

            visitedHistoryHelper.getCallback().onReceiveValue(visitedLinks);
            visitedHistoryHelper.getCallback().onReceiveValue(null);

            loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        } finally {
            webServer.shutdown();
        }
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGetVisitedHistoryCallbackAfterDestroy() throws Throwable {
        GetVisitedHistoryHelper visitedHistoryHelper =
                mContentsClient.getGetVisitedHistoryHelper();
        visitedHistoryHelper.setSaveCallback(true);
        final int callCount = visitedHistoryHelper.getCallCount();
        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();
        loadUrlAsync(awContents, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        visitedHistoryHelper.waitForCallback(callCount);
        assertNotNull(visitedHistoryHelper.getCallback());

        destroyAwContentsOnMainSync(awContents);
        visitedHistoryHelper.getCallback().onReceiveValue(new String[] {"abc.def"});
        visitedHistoryHelper.getCallback().onReceiveValue(null);
    }
}
