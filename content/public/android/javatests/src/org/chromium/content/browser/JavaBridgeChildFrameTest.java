// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

/**
 * Part of the test suite for the WebView's Java Bridge.
 *
 * Ensures that injected objects are exposed to child frames as well as the
 * main frame.
 */
public class JavaBridgeChildFrameTest extends JavaBridgeTestBase {
    private class TestController extends Controller {
        private String mStringValue;

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
        // In the case that the test fails (i.e. the child frame doesn't get the injected object,
        // the call to testController.setStringValue in the child frame's onload handler will
        // not be made.
        loadDataSync(getContentView(),
                "<html><head></head><body>" +
                "<iframe id=\"childFrame\" onload=\"testController.setStringValue('PASS');\" />" +
                "</body></html>", "text/html", false);
        assertEquals("PASS", mTestController.waitForStringValue());
    }
}
