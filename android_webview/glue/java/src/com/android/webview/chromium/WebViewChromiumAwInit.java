// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.AwTracingController;

class WebViewChromiumAwInit {
    private final WebViewChromiumFactoryProvider mFactory;

    WebViewChromiumAwInit(WebViewChromiumFactoryProvider factory) {
        mFactory = factory;
        // Do not make calls into 'factory' in this ctor - this ctor is called from the
        // WebViewChromiumFactoryProvider ctor, so 'factory' is not properly initialized yet.
    }

    // Allows down-stream to override this.
    protected void startChromiumLocked() {}

    AwTracingController getTracingControllerOnUiThread() {
        synchronized (mFactory.mLock) {
            mFactory.ensureChromiumStartedLocked(true);
            return mFactory.getBrowserContextOnUiThread().getTracingController();
        }
    }
}
