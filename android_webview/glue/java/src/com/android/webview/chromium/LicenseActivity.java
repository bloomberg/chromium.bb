// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

/**
 * Activity for displaying WebView OSS licenses.
 */
public class LicenseActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setDataAndType(
                Uri.parse(LicenseContentProvider.LICENSES_URI),
                LicenseContentProvider.LICENSES_CONTENT_TYPE);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        // Try both AOSP and WebView Google package names, as we don't know which one is used.
        int titleId = getResources().getIdentifier(
                "license_activity_title", "string", "com.android.webview");
        if (titleId == 0) {
            titleId = getResources().getIdentifier(
                    "license_activity_title", "string", "com.google.android.webview");
        }
        if (titleId != 0) {
            intent.putExtra(Intent.EXTRA_TITLE, getString(titleId));
        }
        intent.setPackage("com.android.htmlviewer");

        try {
            startActivity(intent);
            finish();
        } catch (ActivityNotFoundException e) {
            Log.e("WebView", "Failed to find viewer", e);
            finish();
        }
    }
}
