// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.webkit.ConsoleMessage;

import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;

class TestAwContentsClient extends NullContentsClient {
    private String mUpdatedTitle;
    private OnPageStartedHelper mOnPageStartedHelper;
    private OnPageFinishedHelper mOnPageFinishedHelper;
    private OnReceivedErrorHelper mOnReceivedErrorHelper;
    private OnEvaluateJavaScriptResultHelper mOnEvaluateJavaScriptResultHelper;
    private AddMessageToConsoleHelper mAddMessageToConsoleHelper;

    public TestAwContentsClient() {
        mOnPageStartedHelper = new OnPageStartedHelper();
        mOnPageFinishedHelper = new OnPageFinishedHelper();
        mOnReceivedErrorHelper = new OnReceivedErrorHelper();
        mOnEvaluateJavaScriptResultHelper = new OnEvaluateJavaScriptResultHelper();
        mAddMessageToConsoleHelper = new AddMessageToConsoleHelper();
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

    public AddMessageToConsoleHelper getAddMessageToConsoleHelper() {
        return mAddMessageToConsoleHelper;
    }

    @Override
    public void onUpdateTitle(String title) {
        mUpdatedTitle = title;
    }

    public String getUpdatedTitle() {
        return mUpdatedTitle;
    }

    @Override
    public void onPageStarted(String url) {
        mOnPageStartedHelper.notifyCalled(url);
    }

    @Override
    public void onPageFinished(String url) {
        mOnPageFinishedHelper.notifyCalled(url);
    }

    @Override
    public void onReceivedError(int errorCode, String description, String failingUrl) {
        mOnReceivedErrorHelper.notifyCalled(errorCode, description, failingUrl);
    }

    @Override
    public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
        mAddMessageToConsoleHelper.notifyCalled(consoleMessage.messageLevel().ordinal(),
                consoleMessage.message(), consoleMessage.lineNumber(), consoleMessage.sourceId());
        return false;
    }

    public class AddMessageToConsoleHelper extends CallbackHelper {
        private int mLevel;
        private String mMessage;
        private int mLineNumber;
        private String mSourceId;

        public int getLevel() {
            assert getCallCount() > 0;
            return mLevel;
        }

        public String getMessage() {
            assert getCallCount() > 0;
            return mMessage;
        }

        public int getLineNumber() {
            assert getCallCount() > 0;
            return mLineNumber;
        }

        public String getSourceId() {
            assert getCallCount() > 0;
            return mSourceId;
        }

        void notifyCalled(int level, String message, int lineNumer, String sourceId) {
            mLevel = level;
            mMessage = message;
            mLineNumber = lineNumer;
            mSourceId = sourceId;
            notifyCalled();
        }
    }
}
