// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

/**
 * Helper class used to fix up resource ids.
 * This is mostly a copy of the code in frameworks/base/core/java/android/app/LoadedApk.java.
 * TODO: Remove if a cleaner mechanism is provided (either public API or AAPT is changed to generate
 * this code).
 */
class ResourceRewriter {
    /**
     * Rewrite the R 'constants' for the WebView library apk.
     */
    public static void rewriteRValues(final int packageId) {
        // TODO: We should use jarjar to remove the redundant R classes here, but due
        // to a bug in jarjar it's not possible to rename classes with '$' in their name.
        // See b/15684775.
        com.android.webview.chromium.R.onResourcesLoaded(packageId);
        org.chromium.ui.R.onResourcesLoaded(packageId);
        org.chromium.content.R.onResourcesLoaded(packageId);
    }
}
