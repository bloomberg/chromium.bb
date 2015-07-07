// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.WebViewDatabase;

import org.chromium.android_webview.AwFormDatabase;
import org.chromium.android_webview.HttpAuthDatabase;

/**
 * Chromium implementation of WebViewDatabase -- forwards calls to the
 * chromium internal implementation.
 */
@SuppressWarnings("deprecation")
final class WebViewDatabaseAdapter extends WebViewDatabase {
    private HttpAuthDatabase mHttpAuthDatabase;

    public WebViewDatabaseAdapter(HttpAuthDatabase httpAuthDatabase) {
        mHttpAuthDatabase = httpAuthDatabase;
    }

    @Override
    public boolean hasUsernamePassword() {
        // This is a deprecated API: intentional no-op.
        return false;
    }

    @Override
    public void clearUsernamePassword() {
        // This is a deprecated API: intentional no-op.
    }

    @Override
    public boolean hasHttpAuthUsernamePassword() {
        return mHttpAuthDatabase.hasHttpAuthUsernamePassword();
    }

    @Override
    public void clearHttpAuthUsernamePassword() {
        mHttpAuthDatabase.clearHttpAuthUsernamePassword();
    }

    @Override
    public boolean hasFormData() {
        return AwFormDatabase.hasFormData();
    }

    @Override
    public void clearFormData() {
        AwFormDatabase.clearFormData();
    }
}
