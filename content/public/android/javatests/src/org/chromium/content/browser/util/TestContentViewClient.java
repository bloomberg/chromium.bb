// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.util;


import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content.browser.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.util.TestCallbackHelperContainer.OnReceivedErrorHelper;

/**
 * The default ContentViewClient used by ContentView tests.
 * <p>
 * Tests that need to supply their own ContentViewClient should do that
 * by extending this one.
 */
public class TestContentViewClient extends ContentViewClient {

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
        mOnPageStartedHelper.notifyCalled(url);
    }

    @Override
    public void onPageFinished(String url) {
        super.onPageFinished(url);
        mOnPageFinishedHelper.notifyCalled(url);
    }

    @Override
    public void onReceivedError(int errorCode, String description, String failingUrl) {
        super.onReceivedError(errorCode, description, failingUrl);
        mOnReceivedErrorHelper.notifyCalled(errorCode, description, failingUrl);
    }

    @Override
    public void onEvaluateJavaScriptResult(int id, String jsonResult) {
        super.onEvaluateJavaScriptResult(id, jsonResult);
        mOnEvaluateJavaScriptResultHelper.notifyCalled(id, jsonResult);
    }
}
