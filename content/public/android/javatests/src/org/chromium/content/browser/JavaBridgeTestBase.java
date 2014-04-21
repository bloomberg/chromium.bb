// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import junit.framework.Assert;

/**
 * Common functionality for testing the Java Bridge.
 */
public class JavaBridgeTestBase extends ContentViewTestBase {
    protected class Controller {
        private boolean mIsResultReady;

        protected synchronized void notifyResultIsReady() {
            mIsResultReady = true;
            notify();
        }
        protected synchronized void waitForResult() {
            while (!mIsResultReady) {
                try {
                    wait(5000);
                } catch (Exception e) {
                    continue;
                }
                if (!mIsResultReady) {
                    Assert.fail("Wait timed out");
                }
            }
            mIsResultReady = false;
        }
    }

    protected void executeJavaScript(final String script) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                // When a JavaScript URL is executed, if the value of the last
                // expression evaluated is not 'undefined', this value is
                // converted to a string and used as the new document for the
                // frame. We don't want this behaviour, so wrap the script in
                // an anonymous function.
                getContentViewCore().loadUrl(new LoadUrlParams(
                        "javascript:(function() { " + script + " })()"));
            }
        });
    }
}
