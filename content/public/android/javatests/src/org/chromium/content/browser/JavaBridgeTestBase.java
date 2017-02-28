// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.util.Log;

import junit.framework.Assert;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.lang.annotation.Annotation;

/**
 * Common functionality for testing the Java Bridge.
 */
public class JavaBridgeTestBase extends ContentShellTestBase {

    protected TestCallbackHelperContainer mTestCallbackHelperContainer;

    /**
     * Sets up the ContentView. Intended to be called from setUp().
     */
    private void setUpContentView() throws Exception {
        // This starts the activity, so must be called on the test thread.
        final ContentShellActivity activity = launchContentShellWithUrl(
                UrlUtils.encodeHtmlDataUri("<html><head></head><body>test</body></html>"));

        waitForActiveShellToBeDoneLoading();

        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mTestCallbackHelperContainer = new TestCallbackHelperContainer(
                            activity.getActiveContentViewCore());
                }
            });
        } catch (Throwable e) {
            throw new RuntimeException(
                    "Failed to set up ContentView: " + Log.getStackTraceString(e));
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        setUpContentView();
    }

    @SuppressFBWarnings("CHROMIUM_SYNCHRONIZED_METHOD")
    protected static class Controller {
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
                getWebContents().getNavigationController().loadUrl(
                        new LoadUrlParams("javascript:(function() { " + script + " })()"));
            }
        });
    }

    protected void injectObjectAndReload(final Object object, final String name) throws Exception {
        injectObjectAndReload(object, name, null);
    }

    protected void injectObjectAndReload(final Object object, final String name,
            final Class<? extends Annotation> requiredAnnotation) throws Exception {
        injectObjectsAndReload(object, name, null, null, requiredAnnotation);
    }

    protected void injectObjectsAndReload(final Object object1, final String name1,
            final Object object2, final String name2,
            final Class<? extends Annotation> requiredAnnotation) throws Exception {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    getContentViewCore().addPossiblyUnsafeJavascriptInterface(object1,
                            name1, requiredAnnotation);
                    if (object2 != null && name2 != null) {
                        getContentViewCore().addPossiblyUnsafeJavascriptInterface(object2,
                                name2, requiredAnnotation);
                    }
                    getContentViewCore().getWebContents().getNavigationController().reload(true);
                }
            });
            onPageFinishedHelper.waitForCallback(currentCallCount);
        } catch (Throwable e) {
            throw new RuntimeException(
                    "Failed to injectObjectsAndReload: " + Log.getStackTraceString(e));
        }
    }

    protected void synchronousPageReload() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().getWebContents().getNavigationController().reload(true);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
    }
}
