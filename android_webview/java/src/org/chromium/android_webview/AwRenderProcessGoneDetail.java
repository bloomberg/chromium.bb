// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

/**
 * This class provides more specific information about why the render process
 * exited. It is peer of android.webkit.RenderProcessGoneDetail.
 */
public class AwRenderProcessGoneDetail {
    private final boolean mDidCrash;

    public AwRenderProcessGoneDetail(boolean didCrash) {
        mDidCrash = didCrash;
    }

    public boolean didCrash() {
        return mDidCrash;
    }
}
