// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import org.chromium.base.JNINamespace;

@JNINamespace("cronet")
public final class MockUrlRequestJobUtil {
    /**
     * Adds url interceptors for mock urls that are used in testing.
     */
    public static void addUrlInterceptors() {
        nativeAddUrlInterceptors();
    }

    private static native void nativeAddUrlInterceptors();
}
