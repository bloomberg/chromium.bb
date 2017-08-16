// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content.common.ContentSwitches;
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

    private static final String DATA = "<html> <script> "
            + "var constraints = {audio: true, video: true};"
            + "var video = document.querySelector('video');"
            + "function successCallback(stream) {"
            + "  window.document.title = 'grant';"
            + "  if (window.URL) {"
            + "    video.src = window.URL.createObjectURL(stream);"
            + "  } else {"
            + "    video.src = stream;"
            + "  }"
            + "}"
            + "function errorCallback(error){"
            + "  window.document.title = 'deny';"
            + "  console.log('navigator.getUserMedia error: ', error);"
            + "}"
            + "navigator.webkitGetUserMedia(constraints, successCallback, errorCallback)"
            + "</script><body>"
            + "<video autoplay></video>"
            + "</body></html>";

    private TestWebServer mTestWebServer;
    private String mWebRTCPage;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestWebServer = TestWebServer.start();
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
    @DisableIf.Build(sdk_is_greater_than = 22, message = "crbug.com/623921")
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    @RetryOnFailure
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
    @DisableIf.Build(sdk_is_greater_than = 22, message = "crbug.com/614347")
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    @RetryOnFailure
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
        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return title.equals(getTitleOnUiThread(awContents));
            }
        });
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @DisableIf.Build(sdk_is_greater_than = 22, message = "crbug.com/614347")
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    @RetryOnFailure
    public void testDenyAccessByDefault() throws Throwable {
        final OnPermissionRequestHelper helper = new OnPermissionRequestHelper();
        TestAwContentsClient contentsClient =
                new TestAwContentsClient() {
                    @Override
                    public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
                        // Intentionally do nothing with awPermissionRequest.
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

        // Cause AwPermissionRequest to be garbage collected, which should deny
        // the request.
        Runtime.getRuntime().gc();

        // Poll with gc in each iteration to reduce flake.
        pollInstrumentationThread(new Callable<Boolean>() {
            @SuppressFBWarnings("DM_GC")
            @Override
            public Boolean call() throws Exception {
                Runtime.getRuntime().gc();
                return "deny".equals(getTitleOnUiThread(awContents));
            }
        });
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @DisableIf.Build(sdk_is_greater_than = 22, message = "crbug.com/614347")
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    @RetryOnFailure
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
