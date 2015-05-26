// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

/**
 * Helper class used to fix up resource ids.
 */
class ResourceRewriter {
    /**
     * Rewrite the R 'constants' for the WebView library apk.
     */
    public static void rewriteRValues(final int packageId) {
        // This list must be kept up to date to include all R classes depended on directly or
        // indirectly by android_webview_java.
        // TODO(torne): find a better way to do this, http://crbug.com/492166.
        com.android.webview.chromium.R.onResourcesLoaded(packageId);
        org.chromium.android_webview.R.onResourcesLoaded(packageId);
        org.chromium.components.web_contents_delegate_android.R.onResourcesLoaded(packageId);
        org.chromium.content.R.onResourcesLoaded(packageId);
        org.chromium.ui.R.onResourcesLoaded(packageId);
    }
}
