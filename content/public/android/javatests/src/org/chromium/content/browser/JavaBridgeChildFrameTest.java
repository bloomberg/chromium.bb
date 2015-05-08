// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Part of the test suite for the WebView's Java Bridge.
 *
 * Ensures that injected objects are exposed to child frames as well as the
 * main frame.
 */
public class JavaBridgeChildFrameTest extends JavaBridgeTestBase {
    @SuppressFBWarnings("CHROMIUM_SYNCHRONIZED_METHOD")
    private class TestController extends Controller {
        private String mStringValue;

        @SuppressWarnings("unused") // Called via reflection
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }

        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }
    }

    TestController mTestController;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestController = new TestController();
        injectObjectAndReload(mTestController, "testController");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testInjectedObjectPresentInChildFrame() throws Throwable {
        loadDataSync(getWebContents().getNavigationController(),
                "<html><body><iframe></iframe></body></html>", "text/html", false);
        // We are not executing this code as a part of page loading routine to avoid races
        // with internal Blink events that notify Java Bridge about window object updates.
        assertEquals("\"object\"", executeJavaScriptAndGetResult(
                        getWebContents(), "typeof window.frames[0].testController"));
        executeJavaScriptAndGetResult(
                getWebContents(), "window.frames[0].testController.setStringValue('PASS')");
        assertEquals("PASS", mTestController.waitForStringValue());
    }

    // Verify that loading an iframe doesn't ruin JS wrapper of the main page.
    // This is a regression test for the problem described in b/15572824.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMainPageWrapperIsNotBrokenByChildFrame() throws Throwable {
        loadDataSync(getWebContents().getNavigationController(),
                "<html><body><iframe></iframe></body></html>", "text/html", false);
        // In case there is anything wrong with the JS wrapper, an attempt
        // to look up its properties will result in an exception being thrown.
        String script = "(function(){ try {"
                + "  return typeof testController.setStringValue;"
                + "} catch (e) {"
                + "  return e.toString();"
                + "} })()";
        assertEquals("\"function\"",
                executeJavaScriptAndGetResult(getWebContents(), script));
        // Make sure calling a method also works.
        executeJavaScriptAndGetResult(getWebContents(),
                "testController.setStringValue('PASS');");
        assertEquals("PASS", mTestController.waitForStringValue());
    }

    // Verify that parent page and child frame each has own JS wrapper object.
    // Failing to do so exposes parent's context to the child.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testWrapperIsNotSharedWithChildFrame() throws Throwable {
        // Test by setting a custom property on the parent page's injected
        // object and then checking that child frame doesn't see the property.
        loadDataSync(getWebContents().getNavigationController(),
                "<html><head>"
                        + "<script>"
                        + "  window.wProperty = 42;"
                        + "  testController.tcProperty = 42;"
                        + "  function queryProperties(w) {"
                        + "    return w.wProperty + ' / ' + w.testController.tcProperty;"
                        + "  }"
                        + "</script>"
                        + "</head><body><iframe></iframe></body></html>", "text/html", false);
        assertEquals("\"42 / 42\"",
                executeJavaScriptAndGetResult(getWebContents(), "queryProperties(window)"));
        assertEquals("\"undefined / undefined\"",
                executeJavaScriptAndGetResult(getWebContents(),
                        "queryProperties(window.frames[0])"));
    }

    // Regression test for crbug.com/484927 -- make sure that existence of transient
    // objects held by multiple RenderFrames doesn't cause an infinite loop when one
    // of them gets removed.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testRemovingTransientObjectHolders() throws Throwable {
        class Test {
            private Object mInner = new Object();
            // Expecting the inner object to be retrieved twice.
            private CountDownLatch mLatch = new CountDownLatch(2);
            @JavascriptInterface
            public Object getInner() {
                mLatch.countDown();
                return mInner;
            }
            public void waitForInjection() throws Throwable {
                if (!mLatch.await(5, TimeUnit.SECONDS)) {
                    throw new TimeoutException();
                }
            }
        }
        final Test testObject = new Test();

        injectObjectAndReload(testObject, "test");
        loadDataSync(getWebContents().getNavigationController(),
                "<html>"
                + "<body onload='window.inner_ref = test.getInner()'>"
                + "   <iframe id='frame' "
                + "       srcdoc='<body onload=\"window.inner_ref = test.getInner()\"></body>'>"
                + "   </iframe>"
                + "</body></html>", "text/html", false);
        testObject.waitForInjection();
        // Just in case, check that the object wrappers are in place.
        assertEquals("\"object\"",
                executeJavaScriptAndGetResult(getWebContents(), "typeof inner_ref"));
        assertEquals("\"object\"",
                executeJavaScriptAndGetResult(getWebContents(),
                        "typeof window.frames[0].inner_ref"));
        // Remove the iframe, this will trigger a removal of RenderFrame, which was causing
        // the bug condition, as the transient object still has a holder -- the main window.
        assertEquals("{}",
                executeJavaScriptAndGetResult(getWebContents(),
                        "(function(){ "
                        + "var f = document.getElementById('frame');"
                        + "f.parentNode.removeChild(f); return f; })()"));
        // Just in case, check that the remaining wrapper is still accessible.
        assertEquals("\"object\"",
                executeJavaScriptAndGetResult(getWebContents(),
                        "typeof inner_ref"));
    }

    private String executeJavaScriptAndGetResult(final WebContents webContents,
            final String script) throws Throwable {
        final String[] result = new String[1];
        class ResultCallback extends JavaBridgeTestBase.Controller
                implements JavaScriptCallback {
            @Override
            public void handleJavaScriptResult(String jsonResult) {
                result[0] = jsonResult;
                notifyResultIsReady();
            }
        }
        final ResultCallback resultCallback = new ResultCallback();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                webContents.evaluateJavaScript(script, resultCallback);
            }
        });
        resultCallback.waitForResult();
        return result[0];
    }

    /**
     * Loads data on the UI thread and blocks until onPageFinished is called.
     */
    private void loadDataSync(final NavigationController navigationController, final String data,
            final String mimeType, final boolean isBase64Encoded) throws Throwable {
        loadUrl(navigationController, mTestCallbackHelperContainer,
                LoadUrlParams.createLoadDataParams(data, mimeType, isBase64Encoded));
    }
}
