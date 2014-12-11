// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.util.Log;

/**
 * Used to track unimplemented methods.
 * TODO: remove this when all WebView APIs have been implemented.
 */
public class UnimplementedWebViewApi {
    private static final String TAG = "UnimplementedWebViewApi";

    private static class UnimplementedWebViewApiException extends UnsupportedOperationException {
        public UnimplementedWebViewApiException() {
            super();
        }
    }

    private static final boolean THROW = false;
    // By default we keep the traces down to one frame to reduce noise, but for debugging it might
    // be useful to set this to true.
    private static final boolean FULL_TRACE = false;

    public static void invoke() throws UnimplementedWebViewApiException {
        if (THROW) {
            throw new UnimplementedWebViewApiException();
        } else {
            if (FULL_TRACE) {
                Log.w(TAG, "Unimplemented WebView method called in: "
                                + Log.getStackTraceString(new Throwable()));
            } else {
                StackTraceElement[] trace = new Throwable().getStackTrace();
                // The stack trace [0] index is this method (invoke()).
                StackTraceElement unimplementedMethod = trace[1];
                StackTraceElement caller = trace[2];
                Log.w(TAG, "Unimplemented WebView method " + unimplementedMethod.getMethodName()
                                + " called from: " + caller.toString());
            }
        }
    }
}
