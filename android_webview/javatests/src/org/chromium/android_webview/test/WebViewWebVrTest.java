// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.JavaScriptUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test that enabling and attempting to use WebVR neither causes any crashes
 * nor returns any VRDisplays.
 */
public class WebViewWebVrTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private ContentViewCore mContentViewCore;

    protected void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mContentViewCore = mTestContainerView.getContentViewCore();
        enableJavaScriptOnUiThread(mTestContainerView.getAwContents());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-webvr")
    public void testWebVrNotFunctional() throws Throwable {
        loadUrlSync(mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(),
                "file:///android_asset/webvr_not_functional_test.html");
        // Poll the boolean to know when the promise resolves
        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                String result = "false";
                try {
                    result = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            mContentViewCore.getWebContents(), "promiseResolved", 100,
                            TimeUnit.MILLISECONDS);
                } catch (InterruptedException | TimeoutException e) {
                    // Expected to happen regularly, do nothing
                }
                return Boolean.parseBoolean(result);
            }
        });

        // Assert that the promise resolved instead of rejecting, but returned
        // 0 VRDisplays
        assertTrue(JavaScriptUtils
                           .executeJavaScriptAndWaitForResult(mContentViewCore.getWebContents(),
                                   "numDisplays", 100, TimeUnit.MILLISECONDS)
                           .equals("0"));
    }
}
