// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium.reflection;

import org.chromium.android_webview.AwContentsStatics;
import org.chromium.base.annotations.UsedByReflection;

/**
 * Entry points for reflection into Android WebView internals for static configuration.
 *
 * Methods in this class may be removed, but they should not be changed in any incompatible way.
 * Methods should be removed when we no longer wish to expose the functionality.
 */
@UsedByReflection("")
public class WebViewConfig {
    @UsedByReflection("")
    public static void disableSafeBrowsing() {
        AwContentsStatics.setSafeBrowsingEnabled(false);
    }
}
