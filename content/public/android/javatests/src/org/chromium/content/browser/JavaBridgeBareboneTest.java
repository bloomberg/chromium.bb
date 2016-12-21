// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Common functionality for testing the Java Bridge.
 */
public class JavaBridgeBareboneTest extends ContentShellTestBase {
    private TestCallbackHelperContainer mTestCallbackHelperContainer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        launchContentShellWithUrl(
                UrlUtils.encodeHtmlDataUri("<html><head></head><body>test</body></html>"));
        waitForActiveShellToBeDoneLoading();
        mTestCallbackHelperContainer = new TestCallbackHelperContainer(getContentViewCore());
    }

    private void injectDummyObject(final String name) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().addPossiblyUnsafeJavascriptInterface(
                        new Object(), name, null);
            }
        });
    }

    private String evaluateJsSync(String jsCode) throws Exception {
        OnEvaluateJavaScriptResultHelper javascriptHelper = new OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(getWebContents(), jsCode);
        javascriptHelper.waitUntilHasValue();
        return javascriptHelper.getJsonResultAndClear();
    }

    private void reloadSync() throws Throwable {
        OnPageFinishedHelper pageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = pageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getWebContents().getNavigationController().reload(true);
            }
        });
        pageFinishedHelper.waitForCallback(currentCallCount);
    }

    // If inection happens before evaluating any JS code, then the first evaluation
    // triggers the same condition as page reload, which causes Java Bridge to add
    // a JS wrapper.
    // This contradicts the statement of our documentation that injected objects are
    // only available after the next page (re)load, but it is too late to fix that,
    // as existing apps may implicitly rely on this behaviour, something like:
    //
    //   webView.loadUrl(...);
    //   webView.addJavascriptInterface(new Foo(), "foo");
    //   webView.loadUrl("javascript:foo.bar();void(0);");
    //
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testImmediateAddition() throws Throwable {
        injectDummyObject("testObject");
        assertEquals("\"object\"", evaluateJsSync("typeof testObject"));
    }

    // Now here, we evaluate some JS before injecting the object, and this behaves as
    // expected.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testNoImmediateAdditionAfterJSEvaluation() throws Throwable {
        evaluateJsSync("true");
        injectDummyObject("testObject");
        assertEquals("\"undefined\"", evaluateJsSync("typeof testObject"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testImmediateAdditionAfterReload() throws Throwable {
        reloadSync();
        injectDummyObject("testObject");
        assertEquals("\"object\"", evaluateJsSync("typeof testObject"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReloadAfterAddition() throws Throwable {
        injectDummyObject("testObject");
        reloadSync();
        assertEquals("\"object\"", evaluateJsSync("typeof testObject"));
    }
}
