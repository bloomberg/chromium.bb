// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import android.content.Intent;
import android.support.test.rule.ActivityTestRule;
import android.webkit.WebView;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.webview_ui_test.R;
import org.chromium.webview_ui_test.WebViewUiTestActivity;

/**
 * WebViewUiTestRule provides ways to synchronously loads file URL or javascript.
 *
 * Note that this must be run on test thread.
 *
 */
public class WebViewUiTestRule extends ActivityTestRule<WebViewUiTestActivity> {

    private WebViewSyncWrapper mSyncWrapper;
    private String mLayout;

    public WebViewUiTestRule(Class<WebViewUiTestActivity> activityClass) {
        super(activityClass);
    }

    @Override
    protected void afterActivityLaunched() {
        mSyncWrapper = new WebViewSyncWrapper((WebView) getActivity().findViewById(R.id.webview));
        super.afterActivityLaunched();
    }

    @Override
    public Statement apply(Statement base, Description desc) {
        UseLayout a = desc.getAnnotation(UseLayout.class);
        if (a != null) {
            mLayout = a.value();
        }
        return super.apply(base, desc);
    }

    @Override
    public WebViewUiTestActivity launchActivity(Intent i) {
        if (mLayout != null && !mLayout.isEmpty()) {
            i.putExtra(WebViewUiTestActivity.EXTRA_TEST_LAYOUT_FILE, mLayout);
        }
        return super.launchActivity(i);
    }

    public WebViewUiTestActivity launchActivity() {
        return launchActivity(new Intent());
    }

    public void loadDataSync(
            String data, String mimeType, String encoding, boolean confirmByJavaScript) {
        try {
            mSyncWrapper.loadDataSync(data, mimeType, encoding, confirmByJavaScript);
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }
    }

    public void loadJavaScriptSync(String js, boolean appendConfirmationJavascript) {
        try {
            mSyncWrapper.loadJavaScriptSync(js, appendConfirmationJavascript);
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }
    }

    public void loadFileSync(String html, boolean confirmByJavaScript) {
        try {
            mSyncWrapper.loadFileSync(html, confirmByJavaScript);
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }
    }
}
