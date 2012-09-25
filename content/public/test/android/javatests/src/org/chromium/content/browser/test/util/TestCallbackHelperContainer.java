// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;


import org.chromium.content.browser.ContentView;

/**
 * This class is used to provide callback hooks for tests and related classes.
 */
public class TestCallbackHelperContainer{
    private TestContentViewClient mTestContentViewClient;
    private TestWebContentsObserver mTestWebContentsObserver;

    public TestCallbackHelperContainer(ContentView contentView) {
        mTestContentViewClient = new TestContentViewClient();
        contentView.getContentViewCore().setContentViewClient(mTestContentViewClient);
        mTestWebContentsObserver = new TestWebContentsObserver(contentView.getContentViewCore());
    }

    public static class OnPageFinishedHelper extends CallbackHelper {
        private String mUrl;
        public void notifyCalled(String url) {
            mUrl = url;
            notifyCalled();
        }
        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }
    }

    public static class OnPageStartedHelper extends CallbackHelper {
        private String mUrl;
        public void notifyCalled(String url) {
            mUrl = url;
            notifyCalled();
        }
        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }
    }

    public static class OnReceivedErrorHelper extends CallbackHelper {
        private int mErrorCode;
        private String mDescription;
        private String mFailingUrl;
        public void notifyCalled(int errorCode, String description, String failingUrl) {
            mErrorCode = errorCode;
            mDescription = description;
            mFailingUrl = failingUrl;
            notifyCalled();
        }
        public int getErrorCode() {
            assert getCallCount() > 0;
            return mErrorCode;
        }
        public String getDescription() {
            assert getCallCount() > 0;
            return mDescription;
        }
        public String getFailingUrl() {
            assert getCallCount() > 0;
            return mFailingUrl;
        }
    }

    public static class OnEvaluateJavaScriptResultHelper extends CallbackHelper {
        private int mId;
        private String mJsonResult;
        public void notifyCalled(int id, String jsonResult) {
            mId = id;
            mJsonResult = jsonResult;
            notifyCalled();
        }
        public int getId() {
            assert getCallCount() > 0;
            return mId;
        }
        public String getJsonResult() {
            assert getCallCount() > 0;
            return mJsonResult;
        }
    }

    public OnPageStartedHelper getOnPageStartedHelper() {
        return mTestWebContentsObserver.getOnPageStartedHelper();
    }

    public OnPageFinishedHelper getOnPageFinishedHelper() {
        return mTestWebContentsObserver.getOnPageFinishedHelper();
    }

    public OnReceivedErrorHelper getOnReceivedErrorHelper() {
        return mTestWebContentsObserver.getOnReceivedErrorHelper();
    }

    public OnEvaluateJavaScriptResultHelper getOnEvaluateJavaScriptResultHelper() {
        return mTestContentViewClient.getOnEvaluateJavaScriptResultHelper();
    }
}
