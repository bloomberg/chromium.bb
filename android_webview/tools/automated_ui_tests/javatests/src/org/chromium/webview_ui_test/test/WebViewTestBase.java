// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test;

import android.content.Intent;
import android.test.ActivityInstrumentationTestCase2;

import org.chromium.webview_ui_test.WebViewUiTestActivity;

/**
 * Base class for the WebView UI app.
 */
public abstract class WebViewTestBase extends
        ActivityInstrumentationTestCase2<WebViewUiTestActivity> {

    private WebViewUiTestActivity mActivity;

    public WebViewTestBase() {
        super(WebViewUiTestActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startWebViewUiTestApp();
    }

    /**
     * Starts the WebView UI Test App
     */
    private void startWebViewUiTestApp() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(getInstrumentation().getTargetContext(), WebViewUiTestActivity.class);
        intent.putExtra(WebViewUiTestActivity.EXTRA_TEST_LAYOUT_FILE, getLayout());
        setActivityIntent(intent);
        mActivity = (WebViewUiTestActivity) getActivity();
    }

    /**
     * Returns the activity under test.
     */
    protected WebViewUiTestActivity getWebViewActivity() {
        return mActivity;
    }

    /**
     * Returns the name of the layout used by the test class.
     */
    protected abstract String getLayout();
}