// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.util.Log;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Provides test environment for ContentView Test Shell.
 * This is a helper class for Content Shell tests.
*/
public class ContentViewTestBase extends ContentShellTestBase {

    protected TestCallbackHelperContainer mTestCallbackHelperContainer;

    /**
     * Sets up the ContentView and injects the supplied object. Intended to be called from setUp().
     */
    protected void setUpContentView(final Object object, final String name) throws Exception {
        // This starts the activity, so must be called on the test thread.
        final ContentShellActivity activity = launchContentShellWithUrl(
                UrlUtils.encodeHtmlDataUri("<html><head></head><body>test</body></html>"));

        waitForActiveShellToBeDoneLoading();

        // On the UI thread, load an empty page and wait for it to finish
        // loading so that the Java object is injected.
        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    ContentViewCore viewCore = activity.getActiveContentViewCore();
                    viewCore.addPossiblyUnsafeJavascriptInterface(object, name, null);
                    mTestCallbackHelperContainer = new TestCallbackHelperContainer(viewCore);
                }
            });

            loadDataSync(activity.getActiveContentViewCore(),
                    "<!DOCTYPE html><title></title>", "text/html", false);
        } catch (Throwable e) {
            throw new RuntimeException(
                    "Failed to set up ContentView: " + Log.getStackTraceString(e));
        }
    }

    /**
     * Loads data on the UI thread and blocks until onPageFinished is called.
     * TODO(cramya): Move method to a separate util file once UiUtils.java moves into base.
     */
    protected void loadDataSync(final ContentViewCore contentViewCore, final String data,
            final String mimeType, final boolean isBase64Encoded) throws Throwable {
        loadUrl(contentViewCore, mTestCallbackHelperContainer, LoadUrlParams.createLoadDataParams(
                data, mimeType, isBase64Encoded));
    }
}
