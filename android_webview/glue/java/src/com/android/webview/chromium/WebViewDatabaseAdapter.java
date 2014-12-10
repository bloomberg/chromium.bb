// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.webkit.WebViewDatabase;

import org.chromium.android_webview.AwFormDatabase;
import org.chromium.android_webview.HttpAuthDatabase;

/**
 * Chromium implementation of WebViewDatabase -- forwards calls to the
 * chromium internal implementation.
 */
final class WebViewDatabaseAdapter extends WebViewDatabase {

    private AwFormDatabase mFormDatabase;
    private HttpAuthDatabase mHttpAuthDatabase;

    public WebViewDatabaseAdapter(AwFormDatabase formDatabase, HttpAuthDatabase httpAuthDatabase) {
        mFormDatabase = formDatabase;
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
        return mFormDatabase.hasFormData();
    }

    @Override
    public void clearFormData() {
        mFormDatabase.clearFormData();
    }
}
