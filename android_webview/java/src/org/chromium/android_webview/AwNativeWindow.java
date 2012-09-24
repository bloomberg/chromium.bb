// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.Intent;

import org.chromium.ui.gfx.NativeWindow;

public class AwNativeWindow extends NativeWindow {
    public AwNativeWindow(Context context) {
        super(context);
    }

    // The following stubs are required for displaying a file picker which is
    // unsupported in the WebView.
    public void sendBroadcast(Intent intent) {
    }

    public boolean showIntent(Intent intent, IntentCallback callback, String error) {
        return false;
    }

    public void showError(String error) {
    }
}
