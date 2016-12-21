// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;
import android.webkit.JavascriptInterface;

import org.chromium.android_webview.AwContents;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;

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
        final String html = "<html>Hello World</html>";
        final TestAwContentsClient client2 = new TestAwContentsClient();
        final AwTestContainerView view2 = createAwTestContainerViewOnMainSync(client2);
        final AwContents awContents = mTestContainerView.getAwContents();

        @SuppressFBWarnings("UMAC_UNCALLABLE_METHOD_OF_ANONYMOUS_CLASS")
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
                    // leave others functioning. Note that we must do this asynchronously,
                    // as Blink thread is currently blocked waiting for this method to finish.
                    loadDataAsync(view2.getAwContents(), html, "text/html", false);
                } catch (Throwable t) {
                    throw new RuntimeException(t);
                }
            }
        }

        enableJavaScriptOnUiThread(awContents);
        runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    awContents.addJavascriptInterface(new Test(), "test");
            }
        });

        loadDataSync(awContents, mContentsClient.getOnPageFinishedHelper(), html,
                "text/html", false);

        // Ensure the JS interface object is there, and invoke the test method.
        assertEquals("\"function\"", executeJavaScriptAndWaitForResult(
                awContents, mContentsClient, "typeof test.destroy"));
        int currentCallCount = client2.getOnPageFinishedHelper().getCallCount();
        awContents.evaluateJavaScriptForTests("test.destroy()", null);

        client2.getOnPageFinishedHelper().waitForCallback(currentCallCount);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTwoWebViewsCreatedSimultaneously() throws Throwable {
        final AwContents awContents1 = mTestContainerView.getAwContents();
        final TestAwContentsClient client2 = new TestAwContentsClient();
        final AwTestContainerView view2 = createAwTestContainerViewOnMainSync(client2);
        final AwContents awContents2 = view2.getAwContents();

        enableJavaScriptOnUiThread(awContents1);
        enableJavaScriptOnUiThread(awContents2);

        class Test {
            Test(int value) {
                mValue = value;
            }
            @SuppressFBWarnings("UMAC_UNCALLABLE_METHOD_OF_ANONYMOUS_CLASS")
            @JavascriptInterface
            public int getValue() {
                return mValue;
            }
            private int mValue;
        }

        runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    awContents1.addJavascriptInterface(new Test(1), "test");
                    awContents2.addJavascriptInterface(new Test(2), "test");
                }
        });
        final String html = "<html>Hello World</html>";
        loadDataSync(awContents1, mContentsClient.getOnPageFinishedHelper(), html,
                "text/html", false);
        loadDataSync(awContents2, client2.getOnPageFinishedHelper(), html,
                "text/html", false);

        assertEquals("1",
                executeJavaScriptAndWaitForResult(awContents1, mContentsClient, "test.getValue()"));
        assertEquals("2",
                executeJavaScriptAndWaitForResult(awContents2, client2, "test.getValue()"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTwoWebViewsSecondCreatedAfterLoadingInFirst() throws Throwable {
        final AwContents awContents1 = mTestContainerView.getAwContents();
        enableJavaScriptOnUiThread(awContents1);

        class Test {
            Test(int value) {
                mValue = value;
            }
            @SuppressFBWarnings("UMAC_UNCALLABLE_METHOD_OF_ANONYMOUS_CLASS")
            @JavascriptInterface
            public int getValue() {
                return mValue;
            }
            private int mValue;
        }

        runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    awContents1.addJavascriptInterface(new Test(1), "test");
                }
        });
        final String html = "<html>Hello World</html>";
        loadDataSync(awContents1, mContentsClient.getOnPageFinishedHelper(), html,
                "text/html", false);
        assertEquals("1",
                executeJavaScriptAndWaitForResult(awContents1, mContentsClient, "test.getValue()"));

        final TestAwContentsClient client2 = new TestAwContentsClient();
        final AwTestContainerView view2 = createAwTestContainerViewOnMainSync(client2);
        final AwContents awContents2 = view2.getAwContents();
        enableJavaScriptOnUiThread(awContents2);

        runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    awContents2.addJavascriptInterface(new Test(2), "test");
                }
        });
        loadDataSync(awContents2, client2.getOnPageFinishedHelper(), html,
                "text/html", false);

        assertEquals("1",
                executeJavaScriptAndWaitForResult(awContents1, mContentsClient, "test.getValue()"));
        assertEquals("2",
                executeJavaScriptAndWaitForResult(awContents2, client2, "test.getValue()"));
    }
}
