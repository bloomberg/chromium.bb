// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.JavaScriptCallback;

/**
 * Part of the test suite for the WebView's Java Bridge.
 *
 * Ensures that injected objects are exposed to child frames as well as the
 * main frame.
 */
public class JavaBridgeChildFrameTest extends JavaBridgeTestBase {
    private class TestController extends Controller {
        private String mStringValue;

    @SuppressWarnings("unused")  // Called via reflection
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
        setUpContentView(mTestController, "testController");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testInjectedObjectPresentInChildFrame() throws Throwable {
        loadDataSync(getContentViewCore(),
                "<html><body><iframe></iframe></body></html>", "text/html", false);
        // We are not executing this code as a part of page loading routine to avoid races
        // with internal Blink events that notify Java Bridge about window object updates.
        assertEquals("\"object\"", executeJavaScriptAndGetResult(
                        getContentViewCore(), "typeof window.frames[0].testController"));
        executeJavaScriptAndGetResult(
                getContentViewCore(), "window.frames[0].testController.setStringValue('PASS')");
        assertEquals("PASS", mTestController.waitForStringValue());
    }

    // Verify that loading an iframe doesn't ruin JS wrapper of the main page.
    // This is a regression test for the problem described in b/15572824.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMainPageWrapperIsNotBrokenByChildFrame() throws Throwable {
        loadDataSync(getContentViewCore(),
                "<html><body><iframe></iframe></body></html>", "text/html", false);
        // In case there is anything wrong with the JS wrapper, an attempt
        // to look up its properties will result in an exception being thrown.
        String script =
                "(function(){ try {" +
                "  return typeof testController.setStringValue;" +
                "} catch (e) {" +
                "  return e.toString();" +
                "} })()";
        assertEquals("\"function\"",
                executeJavaScriptAndGetResult(getContentViewCore(), script));
        // Make sure calling a method also works.
        executeJavaScriptAndGetResult(getContentViewCore(),
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
        loadDataSync(getContentViewCore(),
                "<html><head>" +
                "<script>" +
                "  window.wProperty = 42;" +
                "  testController.tcProperty = 42;" +
                "  function queryProperties(w) {" +
                "    return w.wProperty + ' / ' + w.testController.tcProperty;" +
                "  }" +
                "</script>" +
                "</head><body><iframe></iframe></body></html>", "text/html", false);
        assertEquals("\"42 / 42\"",
                executeJavaScriptAndGetResult(getContentViewCore(), "queryProperties(window)"));
        assertEquals("\"undefined / undefined\"",
                executeJavaScriptAndGetResult(getContentViewCore(),
                        "queryProperties(window.frames[0])"));
    }

    private String executeJavaScriptAndGetResult(final ContentViewCore contentViewCore,
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
                contentViewCore.evaluateJavaScript(script, resultCallback);
            }
        });
        resultCallback.waitForResult();
        return result[0];
    }
}
