// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/**
 * Test MediaAccessPermissionRequest.
 */
public class MediaAccessPermissionRequestTest extends AwTestBase {
    private static class OnPermissionRequestHelper extends CallbackHelper {
        private boolean mCanceled;

        public void notifyCanceled() {
            mCanceled = true;
            notifyCalled();
        }

        public boolean canceled() {
            return mCanceled;
        }
    }

    private static final String DATA = "<html> <script> " +
            "var constraints = {audio: true, video: true};" +
            "var video = document.querySelector('video');" +
            "function successCallback(stream) {" +
                "window.document.title = 'grant';" +
                "if (window.URL) {" +
                    "video.src = window.URL.createObjectURL(stream);" +
                "} else {" +
                    "video.src = stream;" +
                "}" +
            "}" +
            "function errorCallback(error){" +
                "window.document.title = 'deny';" +
                "console.log('navigator.getUserMedia error: ', error);" +
            "}" +
            "navigator.webkitGetUserMedia(constraints, successCallback, errorCallback)" +
            " </script><body>" +
            "<video autoplay></video>" +
            "</body></html>";

    private TestWebServer mTestWebServer;
    private String mWebRTCPage;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestWebServer = new TestWebServer(false);
        mWebRTCPage = mTestWebServer.setResponse("/WebRTC", DATA,
                CommonResources.getTextHtmlHeaders(true));
    }

    @Override
    protected void tearDown() throws Exception {
        mTestWebServer.shutdown();
        mTestWebServer = null;
        super.tearDown();
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGrantAccess() throws Throwable {
        final OnPermissionRequestHelper helper = new OnPermissionRequestHelper();
        TestAwContentsClient contentsClient =
                new TestAwContentsClient() {
                    @Override
                    public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
                        awPermissionRequest.grant();
                        helper.notifyCalled();
                    }
                };
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        enableJavaScriptOnUiThread(awContents);
        int callCount = helper.getCallCount();
        loadUrlAsync(awContents, mWebRTCPage, null);
        helper.waitForCallback(callCount);
        pollTitleAs("grant", awContents);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testDenyAccess() throws Throwable {
        final OnPermissionRequestHelper helper = new OnPermissionRequestHelper();
        TestAwContentsClient contentsClient =
                new TestAwContentsClient() {
                    @Override
                    public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
                        awPermissionRequest.deny();
                        helper.notifyCalled();
                    }
                };
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        enableJavaScriptOnUiThread(awContents);
        int callCount = helper.getCallCount();
        loadUrlAsync(awContents, mWebRTCPage, null);
        helper.waitForCallback(callCount);
        pollTitleAs("deny", awContents);
    }

    private void pollTitleAs(final String title, final AwContents awContents)
            throws Exception {
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return title.equals(getTitleOnUiThread(awContents));
            }
        });
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testCancelPermission() throws Throwable {
        final OnPermissionRequestHelper helper = new OnPermissionRequestHelper();
        TestAwContentsClient contentsClient =
                new TestAwContentsClient() {
                    private AwPermissionRequest mRequest;
                    @Override
                    public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
                        assertNull(mRequest);
                        mRequest = awPermissionRequest;
                        // Don't respond and wait for the request canceled.
                        helper.notifyCalled();
                    }
                    @Override
                    public void onPermissionRequestCanceled(
                            AwPermissionRequest awPermissionRequest) {
                        assertNotNull(mRequest);
                        if (mRequest == awPermissionRequest) helper.notifyCanceled();
                        mRequest = null;
                    }
                };
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        enableJavaScriptOnUiThread(awContents);
        int callCount = helper.getCallCount();
        loadUrlAsync(awContents, mWebRTCPage, null);
        helper.waitForCallback(callCount);
        callCount = helper.getCallCount();
        // Load the same page again, the previous request should be canceled.
        loadUrlAsync(awContents, mWebRTCPage, null);
        helper.waitForCallback(callCount);
        assertTrue(helper.canceled());
    }
}
