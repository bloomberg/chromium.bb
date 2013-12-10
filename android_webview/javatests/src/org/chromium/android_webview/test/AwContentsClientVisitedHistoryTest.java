// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.webkit.ValueCallback;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests for AwContentsClient.getVisitedHistory and AwContents.doUpdateVisitedHistory callbacks.
 */
public class AwContentsClientVisitedHistoryTest extends AwTestBase {
    public static class GetVisitedHistoryHelper extends CallbackHelper {
        private ValueCallback<String[]> mCallback;
        private boolean mSaveCallback = false;

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

    public static class DoUpdateVisitedHistoryHelper extends CallbackHelper {
        String mUrl;
        boolean mIsReload;

        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }

        public boolean getIsReload() {
            assert getCallCount() > 0;
            return mIsReload;
        }

        public void notifyCalled(String url, boolean isReload) {
            mUrl = url;
            mIsReload = isReload;
            notifyCalled();
        }
    }

    private static class TestAwContentsClient
            extends org.chromium.android_webview.test.TestAwContentsClient {

        private GetVisitedHistoryHelper mGetVisitedHistoryHelper;
        private DoUpdateVisitedHistoryHelper mDoUpdateVisitedHistoryHelper;

        public TestAwContentsClient() {
            mGetVisitedHistoryHelper = new GetVisitedHistoryHelper();
            mDoUpdateVisitedHistoryHelper = new DoUpdateVisitedHistoryHelper();
        }

        public GetVisitedHistoryHelper getGetVisitedHistoryHelper() {
            return mGetVisitedHistoryHelper;
        }

        public DoUpdateVisitedHistoryHelper getDoUpdateVisitedHistoryHelper() {
            return mDoUpdateVisitedHistoryHelper;
        }

        @Override
        public void getVisitedHistory(ValueCallback<String[]> callback) {
            getGetVisitedHistoryHelper().notifyCalled(callback);
        }

        @Override
        public void doUpdateVisitedHistory(String url, boolean isReload) {
            getDoUpdateVisitedHistoryHelper().notifyCalled(url, isReload);
        }
    }

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testUpdateVisitedHistoryCallback() throws Throwable {
        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        final String path = "/testUpdateVisitedHistoryCallback.html";
        final String html = "testUpdateVisitedHistoryCallback";

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String pageUrl = webServer.setResponse(path, html, null);
            final DoUpdateVisitedHistoryHelper doUpdateVisitedHistoryHelper =
                mContentsClient.getDoUpdateVisitedHistoryHelper();
            int callCount = doUpdateVisitedHistoryHelper.getCallCount();
            loadUrlAsync(awContents, pageUrl);
            doUpdateVisitedHistoryHelper.waitForCallback(callCount);
            assertEquals(pageUrl, doUpdateVisitedHistoryHelper.getUrl());
            assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

            // Reload
            callCount = doUpdateVisitedHistoryHelper.getCallCount();
            loadUrlAsync(awContents, pageUrl);
            doUpdateVisitedHistoryHelper.waitForCallback(callCount);
            assertEquals(pageUrl, doUpdateVisitedHistoryHelper.getUrl());
            assertEquals(true, doUpdateVisitedHistoryHelper.getIsReload());
        } finally {
            if (webServer != null) webServer.shutdown();
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

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String pageUrl = webServer.setResponse(path, html, null);
            loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
            visitedHistoryHelper.waitForCallback(callCount);
            assertNotNull(visitedHistoryHelper.getCallback());

            visitedHistoryHelper.getCallback().onReceiveValue(visitedLinks);
            visitedHistoryHelper.getCallback().onReceiveValue(null);

            loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        } finally {
            if (webServer != null) webServer.shutdown();
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
        loadUrlAsync(awContents, "about:blank");
        visitedHistoryHelper.waitForCallback(callCount);
        assertNotNull(visitedHistoryHelper.getCallback());

        destroyAwContentsOnMainSync(awContents);
        visitedHistoryHelper.getCallback().onReceiveValue(new String[] {"abc.def"});
        visitedHistoryHelper.getCallback().onReceiveValue(null);
    }
}
