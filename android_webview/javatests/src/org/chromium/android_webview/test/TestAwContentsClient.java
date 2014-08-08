// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Picture;
import android.webkit.ConsoleMessage;

import org.chromium.base.ThreadUtils;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;

/**
 * AwContentsClient subclass used for testing.
 */
public class TestAwContentsClient extends NullContentsClient {
    private String mUpdatedTitle;
    private final OnPageStartedHelper mOnPageStartedHelper;
    private final OnPageFinishedHelper mOnPageFinishedHelper;
    private final OnReceivedErrorHelper mOnReceivedErrorHelper;
    private final OnDownloadStartHelper mOnDownloadStartHelper;
    private final OnReceivedLoginRequestHelper mOnReceivedLoginRequestHelper;
    private final OnEvaluateJavaScriptResultHelper mOnEvaluateJavaScriptResultHelper;
    private final AddMessageToConsoleHelper mAddMessageToConsoleHelper;
    private final OnScaleChangedHelper mOnScaleChangedHelper;
    private final PictureListenerHelper mPictureListenerHelper;
    private final ShouldOverrideUrlLoadingHelper mShouldOverrideUrlLoadingHelper;
    private final DoUpdateVisitedHistoryHelper mDoUpdateVisitedHistoryHelper;

    public TestAwContentsClient() {
        super(ThreadUtils.getUiThreadLooper());
        mOnPageStartedHelper = new OnPageStartedHelper();
        mOnPageFinishedHelper = new OnPageFinishedHelper();
        mOnReceivedErrorHelper = new OnReceivedErrorHelper();
        mOnDownloadStartHelper = new OnDownloadStartHelper();
        mOnReceivedLoginRequestHelper = new OnReceivedLoginRequestHelper();
        mOnEvaluateJavaScriptResultHelper = new OnEvaluateJavaScriptResultHelper();
        mAddMessageToConsoleHelper = new AddMessageToConsoleHelper();
        mOnScaleChangedHelper = new OnScaleChangedHelper();
        mPictureListenerHelper = new PictureListenerHelper();
        mShouldOverrideUrlLoadingHelper = new ShouldOverrideUrlLoadingHelper();
        mDoUpdateVisitedHistoryHelper = new DoUpdateVisitedHistoryHelper();
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

    public OnDownloadStartHelper getOnDownloadStartHelper() {
        return mOnDownloadStartHelper;
    }

    public OnReceivedLoginRequestHelper getOnReceivedLoginRequestHelper() {
        return mOnReceivedLoginRequestHelper;
    }

    public OnEvaluateJavaScriptResultHelper getOnEvaluateJavaScriptResultHelper() {
        return mOnEvaluateJavaScriptResultHelper;
    }

    public ShouldOverrideUrlLoadingHelper getShouldOverrideUrlLoadingHelper() {
        return mShouldOverrideUrlLoadingHelper;
    }

    public AddMessageToConsoleHelper getAddMessageToConsoleHelper() {
        return mAddMessageToConsoleHelper;
    }

    public DoUpdateVisitedHistoryHelper getDoUpdateVisitedHistoryHelper() {
        return mDoUpdateVisitedHistoryHelper;
    }

    /**
     * Callback helper for onScaleChangedScaled.
     */
    public static class OnScaleChangedHelper extends CallbackHelper {
        private float mPreviousScale;
        private float mCurrentScale;
        public void notifyCalled(float oldScale, float newScale) {
            mPreviousScale = oldScale;
            mCurrentScale = newScale;
            super.notifyCalled();
        }

        public float getOldScale() {
            return mPreviousScale;
        }

        public float getNewScale() {
            return mCurrentScale;
        }

        public float getLastScaleRatio() {
            assert getCallCount() > 0;
            return mCurrentScale / mPreviousScale;
        }
    }

    public OnScaleChangedHelper getOnScaleChangedHelper() {
        return mOnScaleChangedHelper;
    }

    public PictureListenerHelper getPictureListenerHelper() {
        return mPictureListenerHelper;
    }

    @Override
    public void onReceivedTitle(String title) {
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

    /**
     * CallbackHelper for OnDownloadStart.
     */
    public static class OnDownloadStartHelper extends CallbackHelper {
        private String mUrl;
        private String mUserAgent;
        private String mContentDisposition;
        private String mMimeType;
        long mContentLength;

        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }

        public String getUserAgent() {
            assert getCallCount() > 0;
            return mUserAgent;
        }

        public String getContentDisposition() {
            assert getCallCount() > 0;
            return mContentDisposition;
        }

        public String getMimeType() {
            assert getCallCount() > 0;
            return mMimeType;
        }

        public long getContentLength() {
            assert getCallCount() > 0;
            return mContentLength;
        }

        public void notifyCalled(String url, String userAgent, String contentDisposition,
                String mimeType, long contentLength) {
            mUrl = url;
            mUserAgent = userAgent;
            mContentDisposition = contentDisposition;
            mMimeType = mimeType;
            mContentLength = contentLength;
            notifyCalled();
        }
    }

    @Override
    public void onDownloadStart(String url,
            String userAgent,
            String contentDisposition,
            String mimeType,
            long contentLength) {
        getOnDownloadStartHelper().notifyCalled(url, userAgent, contentDisposition, mimeType,
                contentLength);
    }

    /**
     * CallbackHelper for OnReceivedLoginRequest.
     */
    public static class OnReceivedLoginRequestHelper extends CallbackHelper {
        private String mRealm;
        private String mAccount;
        private String mArgs;

        public String getRealm() {
            assert getCallCount() > 0;
            return mRealm;
        }

        public String getAccount() {
            assert getCallCount() > 0;
            return mAccount;
        }

        public String getArgs() {
            assert getCallCount() > 0;
            return mArgs;
        }

        public void notifyCalled(String realm, String account, String args) {
            mRealm = realm;
            mAccount = account;
            mArgs = args;
            notifyCalled();
        }
    }

    @Override
    public void onReceivedLoginRequest(String realm, String account, String args) {
        getOnReceivedLoginRequestHelper().notifyCalled(realm, account, args);
    }

    @Override
    public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
        mAddMessageToConsoleHelper.notifyCalled(consoleMessage.messageLevel().ordinal(),
                consoleMessage.message(), consoleMessage.lineNumber(), consoleMessage.sourceId());
        return false;
    }

    /**
     * Callback helper for onScaleChangedScaled.
     */
    public static class AddMessageToConsoleHelper extends CallbackHelper {
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

    @Override
    public void onScaleChangedScaled(float oldScale, float newScale) {
        mOnScaleChangedHelper.notifyCalled(oldScale, newScale);
    }

    /**
     * Callback helper for onScaleChangedScaled.
     */
    public static class PictureListenerHelper extends CallbackHelper {
        // Generally null, depending on |invalidationOnly| in enableOnNewPicture()
        private Picture mPicture;

        public Picture getPicture() {
            assert getCallCount() > 0;
            return mPicture;
        }

        void notifyCalled(Picture picture) {
            mPicture = picture;
            notifyCalled();
        }
    }

    @Override
    public void onNewPicture(Picture picture) {
        mPictureListenerHelper.notifyCalled(picture);
    }

    /**
     * Callback helper for onScaleChangedScaled.
     */
    public static class ShouldOverrideUrlLoadingHelper extends CallbackHelper {
        private String mShouldOverrideUrlLoadingUrl;
        private String mPreviousShouldOverrideUrlLoadingUrl;
        private boolean mShouldOverrideUrlLoadingReturnValue = false;
        void setShouldOverrideUrlLoadingUrl(String url) {
            mShouldOverrideUrlLoadingUrl = url;
        }
        void setPreviousShouldOverrideUrlLoadingUrl(String url) {
            mPreviousShouldOverrideUrlLoadingUrl = url;
        }
        void setShouldOverrideUrlLoadingReturnValue(boolean value) {
            mShouldOverrideUrlLoadingReturnValue = value;
        }
        public String getShouldOverrideUrlLoadingUrl() {
            assert getCallCount() > 0;
            return mShouldOverrideUrlLoadingUrl;
        }
        public String getPreviousShouldOverrideUrlLoadingUrl() {
            assert getCallCount() > 1;
            return mPreviousShouldOverrideUrlLoadingUrl;
        }
        public boolean getShouldOverrideUrlLoadingReturnValue() {
            return mShouldOverrideUrlLoadingReturnValue;
        }
        public void notifyCalled(String url) {
            mPreviousShouldOverrideUrlLoadingUrl = mShouldOverrideUrlLoadingUrl;
            mShouldOverrideUrlLoadingUrl = url;
            notifyCalled();
        }
    }

    @Override
    public boolean shouldOverrideUrlLoading(String url) {
        super.shouldOverrideUrlLoading(url);
        boolean returnValue =
            mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingReturnValue();
        mShouldOverrideUrlLoadingHelper.notifyCalled(url);
        return returnValue;
    }


    /**
     * Callback helper for doUpdateVisitedHistory.
     */
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

    @Override
    public void doUpdateVisitedHistory(String url, boolean isReload) {
        getDoUpdateVisitedHistoryHelper().notifyCalled(url, isReload);
    }
}
