// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Picture;
import android.os.Handler;
import android.os.Looper;
import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsClientCallbackHelper;
import org.chromium.android_webview.test.TestAwContentsClient.OnDownloadStartHelper;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedLoginRequestHelper;
import org.chromium.android_webview.test.TestAwContentsClient.PictureListenerHelper;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.parameter.ParameterizedTest;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;

import java.util.concurrent.Callable;

/**
 * Test suite for AwContentsClientCallbackHelper.
 */
@ParameterizedTest.Set  // These are unit tests. No need to repeat for multiprocess.
public class AwContentsClientCallbackHelperTest extends AwTestBase {
    /**
     * Callback helper for OnLoadedResource.
     */
    public static class OnLoadResourceHelper extends CallbackHelper {
        private String mLastLoadedResource;

        public String getLastLoadedResource() {
            assert getCallCount() > 0;
            return mLastLoadedResource;
        }

        public void notifyCalled(String url) {
            mLastLoadedResource = url;
            notifyCalled();
        }
    }

    /**
     * TestAwContentsClient extended with OnLoadResourceHelper.
     */
    public static class TestAwContentsClient
            extends org.chromium.android_webview.test.TestAwContentsClient {

        private final OnLoadResourceHelper mOnLoadResourceHelper;

        public TestAwContentsClient() {
            super();
            mOnLoadResourceHelper = new OnLoadResourceHelper();
        }

        public OnLoadResourceHelper getOnLoadResourceHelper() {
            return mOnLoadResourceHelper;
        }

        @Override
        public void onLoadResource(String url) {
            mOnLoadResourceHelper.notifyCalled(url);
        }
    }

    private static class TestCancelCallbackPoller
            implements AwContentsClientCallbackHelper.CancelCallbackPoller {
        private boolean mCancelled;
        private final CallbackHelper mCallbackHelper = new CallbackHelper();

        public void setCancelled() {
            mCancelled = true;
        }

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }

        @Override
        public boolean cancelAllCallbacks() {
            mCallbackHelper.notifyCalled();
            return mCancelled;
        }
    }

    static final int PICTURE_TIMEOUT = 5000;
    static final String TEST_URL = "www.example.com";
    static final String REALM = "www.example.com";
    static final String ACCOUNT = "account";
    static final String ARGS = "args";
    static final String USER_AGENT = "userAgent";
    static final String CONTENT_DISPOSITION = "contentDisposition";
    static final String MIME_TYPE = "mimeType";
    static final int CONTENT_LENGTH = 42;

    static final float NEW_SCALE = 1.0f;
    static final float OLD_SCALE = 2.0f;
    static final int ERROR_CODE = 2;
    static final String ERROR_MESSAGE = "A horrible thing has occurred!";

    private TestAwContentsClient mContentsClient;
    private AwContentsClientCallbackHelper mClientHelper;
    private TestCancelCallbackPoller mCancelCallbackPoller;
    private Looper mLooper;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mLooper = Looper.getMainLooper();
        mContentsClient = new TestAwContentsClient();
        mClientHelper = new AwContentsClientCallbackHelper(mLooper, mContentsClient);
        mCancelCallbackPoller = new TestCancelCallbackPoller();
        mClientHelper.setCancelCallbackPoller(mCancelCallbackPoller);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnLoadResource() throws Exception {
        OnLoadResourceHelper loadResourceHelper = mContentsClient.getOnLoadResourceHelper();

        int onLoadResourceCount = loadResourceHelper.getCallCount();
        mClientHelper.postOnLoadResource(TEST_URL);
        loadResourceHelper.waitForCallback(onLoadResourceCount);
        assertEquals(TEST_URL, loadResourceHelper.getLastLoadedResource());
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnPageStarted() throws Exception {
        OnPageStartedHelper pageStartedHelper = mContentsClient.getOnPageStartedHelper();

        int onPageStartedCount = pageStartedHelper.getCallCount();
        mClientHelper.postOnPageStarted(TEST_URL);
        pageStartedHelper.waitForCallback(onPageStartedCount);
        assertEquals(TEST_URL, pageStartedHelper.getUrl());
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnDownloadStart() throws Exception {
        OnDownloadStartHelper downloadStartHelper = mContentsClient.getOnDownloadStartHelper();

        int onDownloadStartCount = downloadStartHelper.getCallCount();
        mClientHelper.postOnDownloadStart(TEST_URL, USER_AGENT, CONTENT_DISPOSITION,
                MIME_TYPE, CONTENT_LENGTH);
        downloadStartHelper.waitForCallback(onDownloadStartCount);
        assertEquals(TEST_URL, downloadStartHelper.getUrl());
        assertEquals(USER_AGENT, downloadStartHelper.getUserAgent());
        assertEquals(CONTENT_DISPOSITION, downloadStartHelper.getContentDisposition());
        assertEquals(MIME_TYPE, downloadStartHelper.getMimeType());
        assertEquals(CONTENT_LENGTH, downloadStartHelper.getContentLength());
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnNewPicture() throws Exception {
        final PictureListenerHelper pictureListenerHelper =
                mContentsClient.getPictureListenerHelper();

        final Picture thePicture = new Picture();

        final Callable<Picture> pictureProvider = new Callable<Picture>() {
            @Override
            public Picture call() {
                return thePicture;
            }
        };

        // AwContentsClientCallbackHelper rate limits photo callbacks so two posts in close
        // succession should only result in one callback.
        final int onNewPictureCount = pictureListenerHelper.getCallCount();
        // To trip the rate limiting the second postNewPicture call needs to happen
        // before mLooper processes the first. To do this we run both posts as a single block
        // and we do it in the thread that is processes the callbacks (mLooper).
        Handler mainHandler = new Handler(mLooper);
        Runnable postPictures = new Runnable() {
            @Override
            public void run() {
                mClientHelper.postOnNewPicture(pictureProvider);
                mClientHelper.postOnNewPicture(pictureProvider);
            }
        };
        mainHandler.post(postPictures);

        // We want to check that one and only one callback is fired,
        // First we wait for the first call back to complete, this ensures that both posts have
        // finished.
        pictureListenerHelper.waitForCallback(onNewPictureCount);

        // Then we post a runnable on the callback handler thread. Since both posts have happened
        // and the first callback has happened a second callback (if it exists) must be
        // in the queue before this runnable.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
            }
        });

        // When that runnable has finished we assert that one and only on callback happened.
        assertEquals(thePicture, pictureListenerHelper.getPicture());
        assertEquals(onNewPictureCount + 1, pictureListenerHelper.getCallCount());
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnReceivedLoginRequest() throws Exception {
        OnReceivedLoginRequestHelper receivedLoginRequestHelper =
                mContentsClient.getOnReceivedLoginRequestHelper();

        int onReceivedLoginRequestCount = receivedLoginRequestHelper.getCallCount();
        mClientHelper.postOnReceivedLoginRequest(REALM, ACCOUNT, ARGS);
        receivedLoginRequestHelper.waitForCallback(onReceivedLoginRequestCount);
        assertEquals(REALM, receivedLoginRequestHelper.getRealm());
        assertEquals(ACCOUNT, receivedLoginRequestHelper.getAccount());
        assertEquals(ARGS, receivedLoginRequestHelper.getArgs());
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnReceivedError() throws Exception {
        OnReceivedErrorHelper receivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();

        int onReceivedErrorCount = receivedErrorHelper.getCallCount();
        AwContentsClient.AwWebResourceRequest request = new AwContentsClient.AwWebResourceRequest();
        request.url = TEST_URL;
        request.isMainFrame = true;
        AwContentsClient.AwWebResourceError error = new AwContentsClient.AwWebResourceError();
        error.errorCode = ERROR_CODE;
        error.description = ERROR_MESSAGE;
        mClientHelper.postOnReceivedError(request, error);
        receivedErrorHelper.waitForCallback(onReceivedErrorCount);
        assertEquals(ERROR_CODE, receivedErrorHelper.getErrorCode());
        assertEquals(ERROR_MESSAGE, receivedErrorHelper.getDescription());
        assertEquals(TEST_URL, receivedErrorHelper.getFailingUrl());
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnScaleChangedScaled() throws Exception {
        TestAwContentsClient.OnScaleChangedHelper scaleChangedHelper =
                mContentsClient.getOnScaleChangedHelper();

        int onScaleChangeCount = scaleChangedHelper.getCallCount();
        mClientHelper.postOnScaleChangedScaled(OLD_SCALE, NEW_SCALE);
        scaleChangedHelper.waitForCallback(onScaleChangeCount);
        assertEquals(OLD_SCALE, scaleChangedHelper.getOldScale());
        assertEquals(NEW_SCALE, scaleChangedHelper.getNewScale());
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testCancelCallbackPoller() throws Exception {
        mCancelCallbackPoller.setCancelled();
        CallbackHelper cancelCallbackPollerHelper = mCancelCallbackPoller.getCallbackHelper();
        OnPageStartedHelper pageStartedHelper = mContentsClient.getOnPageStartedHelper();

        int pollCount = pageStartedHelper.getCallCount();
        int onPageStartedCount = pageStartedHelper.getCallCount();
        // Post two callbacks.
        mClientHelper.postOnPageStarted(TEST_URL);
        mClientHelper.postOnPageStarted(TEST_URL);

        // Wait for at least one poll.
        cancelCallbackPollerHelper.waitForCallback(pollCount);

        // Flush main queue.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
            }
        });

        // Neither callback should actually happen.
        assertEquals(onPageStartedCount, pageStartedHelper.getCallCount());
    }
}
