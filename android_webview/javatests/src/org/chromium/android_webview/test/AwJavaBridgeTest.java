// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.JavascriptInterface;

/**
 * Test suite for the WebView specific JavaBridge features.
 */
public class AwJavaBridgeTest extends AwTestBase {

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private AwTestContainerView mTestContainerView;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testDestroyFromJavaObject() throws Throwable {
        final String HTML = "<html>Hello World</html>";
        final TestAwContentsClient client2 = new TestAwContentsClient();
        final AwTestContainerView view2 = createAwTestContainerViewOnMainSync(client2);
        final AwContents awContents = mTestContainerView.getAwContents();

        class Test {
            @JavascriptInterface
            public void destroy() {
                try {
                    runTestOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                awContents.destroy();
                            }
                    });
                    // Destroying one AwContents from within the JS callback should still
                    // leave others functioning.
                    loadDataSync(view2.getAwContents(), client2.getOnPageFinishedHelper(),
                            HTML, "text/html", false);
                } catch (Throwable t) {
                    throw new RuntimeException(t);
                }
            }
        }

        enableJavaScriptOnUiThread(awContents);
        runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    awContents.addPossiblyUnsafeJavascriptInterface(new Test(), "test", null);
            }
        });

        loadDataSync(awContents, mContentsClient.getOnPageFinishedHelper(), HTML,
                "text/html", false);

        // Ensure the JS interface object is there, and invoke the test method.
        assertEquals("\"function\"", executeJavaScriptAndWaitForResult(
                awContents, mContentsClient, "typeof test.destroy"));
        awContents.evaluateJavaScript("test.destroy()", null);

        client2.getOnPageFinishedHelper().waitForCallback(
                client2.getOnPageFinishedHelper().getCallCount());
    }
}
