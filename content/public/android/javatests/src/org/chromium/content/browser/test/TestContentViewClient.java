// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test;


import org.chromium.content.browser.ContentViewClient;

/**
 * The default ContentViewClient used by ContentView tests.
 * <p>
 * Tests that need to supply their own ContentViewClient should do that
 * by extending this one.
 */
public class TestContentViewClient extends ContentViewClient {

    protected static int WAIT_TIMEOUT_SECONDS = 5;

    public static class OnPageFinishedHelper extends CallbackHelper {
        private String mUrl;
        void setUrl(String url) {
            mUrl = url;
        }
        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }
    }

    public static class OnPageStartedHelper extends CallbackHelper {
        private String mUrl;
        void setUrl(String url) {
            mUrl = url;
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
        void setErrorCode(int errorCode) {
            mErrorCode = errorCode;
        }
        void setDescription(String description) {
            mDescription = description;
        }
        void setFailingUrl(String failingUrl) {
            mFailingUrl = failingUrl;
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
        void setId(int id) {
            mId = id;
        }
        void setJsonResult(String jsonResult) {
            mJsonResult = jsonResult;
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

    private OnPageStartedHelper mOnPageStartedHelper;
    private OnPageFinishedHelper mOnPageFinishedHelper;
    private OnReceivedErrorHelper mOnReceivedErrorHelper;
    private OnEvaluateJavaScriptResultHelper mOnEvaluateJavaScriptResultHelper;

    public TestContentViewClient() {
        mOnPageStartedHelper = new OnPageStartedHelper();
        mOnPageFinishedHelper = new OnPageFinishedHelper();
        mOnReceivedErrorHelper = new OnReceivedErrorHelper();
        mOnEvaluateJavaScriptResultHelper = new OnEvaluateJavaScriptResultHelper();
    }

    public OnPageStartedHelper getOnPageStartedHelper() {
        return mOnPageStartedHelper;
    }

    public OnPageFinishedHelper getOnPageFinishedHelper() {
        return mOnPageFinishedHelper;
    }

    public OnReceivedErrorHelper getOnReceivedErrorHelper() {
        return mOnReceivedErrorHelper;
    }

    public OnEvaluateJavaScriptResultHelper getOnEvaluateJavaScriptResultHelper() {
        return mOnEvaluateJavaScriptResultHelper;
    }

    /**
     * ATTENTION!: When overriding the following methods, be sure to call
     * the corresponding methods in the super class. Otherwise
     * {@link CallbackHelper#waitForCallback()} methods will
     * stop working!
     */
    @Override
    public void onPageStarted(String url) {
        super.onPageStarted(url);
        mOnPageStartedHelper.setUrl(url);
        mOnPageStartedHelper.notifyCalled();
    }

    @Override
    public void onPageFinished(String url) {
        super.onPageFinished(url);
        mOnPageFinishedHelper.setUrl(url);
        mOnPageFinishedHelper.notifyCalled();
    }

    @Override
    public void onReceivedError(int errorCode, String description, String failingUrl) {
        super.onReceivedError(errorCode, description, failingUrl);
        mOnReceivedErrorHelper.setErrorCode(errorCode);
        mOnReceivedErrorHelper.setDescription(description);
        mOnReceivedErrorHelper.setFailingUrl(failingUrl);
        mOnReceivedErrorHelper.notifyCalled();
    }

    @Override
    public void onEvaluateJavaScriptResult(int id, String jsonResult) {
        super.onEvaluateJavaScriptResult(id, jsonResult);
        mOnEvaluateJavaScriptResultHelper.setId(id);
        mOnEvaluateJavaScriptResultHelper.setJsonResult(jsonResult);
        mOnEvaluateJavaScriptResultHelper.notifyCalled();
    }
}
