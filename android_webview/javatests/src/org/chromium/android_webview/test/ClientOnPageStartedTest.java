// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;

/**
 * Tests for the ContentViewClient.onPageStarted() method.
 */
public class ClientOnPageStartedTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        setTestAwContentsClient(new TestAwContentsClient());
    }

    private void setTestAwContentsClient(TestAwContentsClient contentsClient) throws Exception {
        mContentsClient = contentsClient;
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageStartedPassesCorrectUrl() throws Throwable {
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();

        String html = "<html><body>Simple page.</body></html>";
        int currentCallCount = onPageStartedHelper.getCallCount();
        loadDataAsync(mAwContents, html, "text/html", false);

        onPageStartedHelper.waitForCallback(currentCallCount);
        assertEquals("data:text/html," + html, onPageStartedHelper.getUrl());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageStartedCalledOnceOnError() throws Throwable {
        class LocalTestClient extends TestAwContentsClient {
            private boolean isOnReceivedErrorCalled = false;
            private boolean isOnPageStartedCalled = false;
            private boolean allowAboutBlank = false;

            @Override
            public void onReceivedError(int errorCode, String description, String failingUrl) {
                assertEquals("onReceivedError called twice for " + failingUrl,
                        false, isOnReceivedErrorCalled);
                isOnReceivedErrorCalled = true;
                assertEquals("onPageStarted not called before onReceivedError for " + failingUrl,
                        true, isOnPageStartedCalled);
                super.onReceivedError(errorCode, description, failingUrl);
            }

            @Override
            public void onPageStarted(String url) {
                if (allowAboutBlank && "about:blank".equals(url)) {
                    super.onPageStarted(url);
                    return;
                }
                assertEquals("onPageStarted called twice for " + url,
                        false, isOnPageStartedCalled);
                isOnPageStartedCalled = true;
                assertEquals("onReceivedError called before onPageStarted for " + url,
                        false, isOnReceivedErrorCalled);
                super.onPageStarted(url);
            }

            void setAllowAboutBlank() {
                allowAboutBlank = true;
            }
        };
        LocalTestClient testContentsClient = new LocalTestClient();
        setTestAwContentsClient(testContentsClient);

        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        String invalidUrl = "http://localhost:7/non_existent";
        loadUrlSync(mAwContents, onPageFinishedHelper, invalidUrl);

        assertEquals(invalidUrl, onReceivedErrorHelper.getFailingUrl());
        assertEquals(invalidUrl, onPageStartedHelper.getUrl());

        // Rather than wait a fixed time to see that another onPageStarted callback isn't issued
        // we load a valid page. Since callbacks arrive sequentially, this will ensure that
        // any extra calls of onPageStarted / onReceivedError will arrive to our client.
        testContentsClient.setAllowAboutBlank();
        loadUrlSync(mAwContents, onPageFinishedHelper, "about:blank");
    }
}
