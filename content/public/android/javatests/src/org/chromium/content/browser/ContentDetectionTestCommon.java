// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.net.Uri;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_shell.ShellViewAndroidDelegate.ContentIntentHandler;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestCommon.TestCommonCallback;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

public class ContentDetectionTestCommon {
    private static final long WAIT_TIMEOUT_SECONDS = scaleTimeout(10);

    private final TestCommonCallback<ContentShellActivity> mCallback;

    private TestCallbackHelperContainer mCallbackHelper;
    private TestContentIntentHandler mContentIntentHandler;

    public ContentDetectionTestCommon(TestCommonCallback<ContentShellActivity> callback) {
        mCallback = callback;
    }

    /**
     * CallbackHelper for OnStartContentIntent.
     */
    private static class OnStartContentIntentHelper extends CallbackHelper {
        private String mIntentUrl;
        public void notifyCalled(String intentUrl) {
            mIntentUrl = intentUrl;
            notifyCalled();
        }
        public String getIntentUrl() {
            assert getCallCount() > 0;
            return mIntentUrl;
        }
    }

    /**
     * ContentIntentHandler impl to test content detection.
     */
    public static class TestContentIntentHandler implements ContentIntentHandler {
        private OnStartContentIntentHelper mOnStartContentIntentHelper;

        public OnStartContentIntentHelper getOnStartContentIntentHelper() {
            if (mOnStartContentIntentHelper == null) {
                mOnStartContentIntentHelper = new OnStartContentIntentHelper();
            }
            return mOnStartContentIntentHelper;
        }

        @Override
        public void onIntentUrlReceived(String intentUrl) {
            mOnStartContentIntentHelper.notifyCalled(intentUrl);
        }
    }

    static String urlForContent(String content) {
        return Uri.encode(content).replaceAll("%20", "+");
    }

    TestCallbackHelperContainer getTestCallbackHelperContainer() {
        if (mCallbackHelper == null) {
            mCallbackHelper =
                    new TestCallbackHelperContainer(mCallback.getContentViewCoreForTestCommon());
        }
        return mCallbackHelper;
    }

    void createTestContentIntentHandler() {
        mContentIntentHandler = new TestContentIntentHandler();
    }

    void setContentHandler() {
        mCallback.getActivityForTestCommon()
                .getShellManager()
                .getActiveShell()
                .getViewAndroidDelegate()
                .setContentIntentHandler(mContentIntentHandler);
    }

    boolean isCurrentTestUrl(String testUrl) {
        return UrlUtils.getIsolatedTestFileUrl(testUrl).equals(
                mCallback.getContentViewCoreForTestCommon().getWebContents().getUrl());
    }

    String scrollAndTapExpectingIntent(String id) throws InterruptedException, TimeoutException {
        OnStartContentIntentHelper onStartContentIntentHelper =
                mContentIntentHandler.getOnStartContentIntentHelper();
        int currentCallCount = onStartContentIntentHelper.getCallCount();

        DOMUtils.clickNode(mCallback.getContentViewCoreForTestCommon(), id);

        onStartContentIntentHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        mCallback.getInstrumentationForTestCommon().waitForIdleSync();
        return onStartContentIntentHelper.getIntentUrl();
    }

    void scrollAndTapNavigatingOut(String id) throws InterruptedException, TimeoutException {
        TestCallbackHelperContainer callbackHelperContainer = getTestCallbackHelperContainer();
        OnPageFinishedHelper onPageFinishedHelper =
                callbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();

        DOMUtils.clickNode(mCallback.getContentViewCoreForTestCommon(), id);

        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        mCallback.getInstrumentationForTestCommon().waitForIdleSync();
    }
}
