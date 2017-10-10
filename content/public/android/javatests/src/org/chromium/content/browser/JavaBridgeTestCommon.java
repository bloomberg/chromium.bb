// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.util.Log;

import org.junit.Assert;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestCommon.TestCommonCallback;

import java.lang.annotation.Annotation;

/**
 * Common functionality implementation for testing the Java Bridge.
 */
public final class JavaBridgeTestCommon {
    protected TestCallbackHelperContainer mTestCallbackHelperContainer;

    private final TestCommonCallback<ContentShellActivity> mCallback;

    public JavaBridgeTestCommon(TestCommonCallback<ContentShellActivity> callback) {
        mCallback = callback;
    }

    @SuppressFBWarnings("CHROMIUM_SYNCHRONIZED_METHOD")
    public static class Controller {
        private static final int RESULT_WAIT_TIME = 5000;

        private boolean mIsResultReady;

        protected synchronized void notifyResultIsReady() {
            mIsResultReady = true;
            notify();
        }

        protected synchronized void waitForResult() {
            while (!mIsResultReady) {
                try {
                    wait(RESULT_WAIT_TIME);
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

    TestCallbackHelperContainer getTestCallBackHelperContainer() {
        return mTestCallbackHelperContainer;
    }

    void setUpContentView() {
        // This starts the activity, so must be called on the test thread.
        final ContentShellActivity activity = mCallback.launchContentShellWithUrlForTestCommon(
                UrlUtils.encodeHtmlDataUri("<html><head></head><body>test</body></html>"));
        mCallback.waitForActiveShellToBeDoneLoadingForTestCommon();

        try {
            mCallback.runOnUiThreadForTestCommon(new Runnable() {
                @Override
                public void run() {
                    mTestCallbackHelperContainer =
                            new TestCallbackHelperContainer(activity.getActiveContentViewCore());
                }
            });
        } catch (Throwable e) {
            throw new RuntimeException(
                    "Failed to set up ContentView: " + Log.getStackTraceString(e));
        }
    }

    void executeJavaScript(final String script) throws Throwable {
        mCallback.runOnUiThreadForTestCommon(new Runnable() {
            @Override
            public void run() {
                // When a JavaScript URL is executed, if the value of the last
                // expression evaluated is not 'undefined', this value is
                // converted to a string and used as the new document for the
                // frame. We don't want this behaviour, so wrap the script in
                // an anonymous function.
                mCallback.getWebContentsForTestCommon().getNavigationController().loadUrl(
                        new LoadUrlParams("javascript:(function() { " + script + " })()"));
            }
        });
    }

    void injectObjectsAndReload(final Object object1, final String name1, final Object object2,
            final String name2, final Class<? extends Annotation> requiredAnnotation)
            throws Exception {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        try {
            mCallback.runOnUiThreadForTestCommon(new Runnable() {
                @Override
                public void run() {
                    WebContents webContents = mCallback.getWebContentsForTestCommon();
                    webContents.addPossiblyUnsafeJavascriptInterface(
                            object1, name1, requiredAnnotation);
                    if (object2 != null && name2 != null) {
                        webContents.addPossiblyUnsafeJavascriptInterface(
                                object2, name2, requiredAnnotation);
                    }
                    webContents.getNavigationController().reload(true);
                }
            });
            onPageFinishedHelper.waitForCallback(currentCallCount);
        } catch (Throwable e) {
            throw new RuntimeException(
                    "Failed to injectObjectsAndReload: " + Log.getStackTraceString(e));
        }
    }

    void synchronousPageReload() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        mCallback.runOnUiThreadForTestCommon(new Runnable() {
            @Override
            public void run() {
                mCallback.getWebContentsForTestCommon().getNavigationController().reload(true);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
    }
}
