// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.Smoke;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.Feature;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.TestContentViewClient;

import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicReference;
import java.util.ArrayList;

/**
 * Test suite for loadUrl().
 */
public class AndroidWebViewLoadUrlTest extends AndroidWebViewTestBase {

    private ContentViewCore createContentViewOnMainSync(final TestContentViewClient client)
            throws Exception {
        final AtomicReference<ContentView> contentView = new AtomicReference<ContentView>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                contentView.set(createContentView(false, client));
                getActivity().setContentViews(contentView.get());
            }
        });
        return contentView.get().getContentViewCore();
    }

    private String getTitleOnUiThread(final ContentViewCore contentViewCore) throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return contentViewCore.getTitle();
            }
        });
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testDataUrl() throws Throwable {
        final String expectedTitle = "dataUrlTest";
        final String dataUrl =
            "data:text/html,<html><head><title>" + expectedTitle + "</title></head></html>";

        final TestContentViewClient contentViewClient = new TestContentViewClient();
        final ContentViewCore contentViewCore = createContentViewOnMainSync(contentViewClient);
        loadUrlSync(contentViewCore, contentViewClient.getOnPageFinishedHelper(), dataUrl);
        assertEquals(expectedTitle, getTitleOnUiThread(contentViewCore));
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testDataUrlBase64() throws Throwable {
        final String expectedTitle = "dataUrlTestBase64";
        final String dataUrl = "data:text/html;base64," +
                "PGh0bWw+PGhlYWQ+PHRpdGxlPmRhdGFVcmxUZXN0QmFzZTY0PC90aXRsZT48" +
                "L2hlYWQ+PC9odG1sPg==";

        final TestContentViewClient contentViewClient = new TestContentViewClient();
        final ContentViewCore contentViewCore = createContentViewOnMainSync(contentViewClient);
        loadUrlSync(contentViewCore, contentViewClient.getOnPageFinishedHelper(), dataUrl);
        assertEquals(expectedTitle, getTitleOnUiThread(contentViewCore));
    }
}
