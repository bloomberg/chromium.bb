// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test;

import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;
import android.test.suitebuilder.annotation.SmallTest;
import android.webkit.WebView;

import org.chromium.webview_ui_test.R;

/**
 * Tests for loadData API.
 */
public class LoadDataTest extends WebViewTestBase {

    private static final long UI_TIMEOUT = 10 * 1000;

    @Override
    protected String getLayout() {
        return "fullscreen_webview";
    }

    /**
     * Test for loading HTML data and verifying it is displayed as expected.
     */
    @SmallTest
    public void testLoadDataHtml() {

        final String headerText = "HELLO WORLD";

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                WebView w = (WebView) getWebViewActivity().findViewById(R.id.webview);
                w.loadData(String.format("<html><body><h1>%s</h1></body></html>", headerText),
                           "text/html", "utf-8");
            }
        });

        UiDevice device = UiDevice.getInstance(getInstrumentation());
        UiObject2 webViewUi = device.findObject(
                By.res(getWebViewActivity().getPackageName(), "webview"));
        assertTrue(webViewUi.wait(Until.hasObject(By.desc(headerText)), UI_TIMEOUT));
    }
}