// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * Delegate for handling callbacks from the IO thread on the IO thread.
 *
 * The shouldInterceptRequest method will be called on the IO thread.
 * You should create a separate instance for every WebContents that requires the
 * provided functionality.
 */
@JNINamespace("android_webview")
public interface AwContentsIoThreadClient {
    // Called on the IO thread.
    @CalledByNative
    InterceptedRequestData shouldInterceptRequest(String url);
}
