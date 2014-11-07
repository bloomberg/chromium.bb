// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import org.chromium.base.JNINamespace;

/**
 * Test utils for MockUrlRequestJobs.
 */
@JNINamespace("cronet")
public final class MockUrlRequestJobUtil {
    enum FailurePhase {
      START,
      READ_ASYNC,
      READ_SYNC,
    };

    /**
     * Adds url interceptors for mock urls that are used in testing.
     */
    public static void addUrlInterceptors() {
        nativeAddUrlInterceptors();
    }

    /**
     * Constructs a mock URL.
     *
     * @param path path to a mock file.
     */
    public static String getMockUrl(String path) {
        return nativeGetMockUrl(path);
    }

    /**
     * Constructs a mock URL that hangs or fails at certain phase.
     *
     * @param path path to a mock file.
     * @param phase at which request fails.
     * @param netError reported by UrlRequestJob. Passing -1, results in hang.
     */
    public static String getMockUrlWithFailure(String path, FailurePhase phase,
            int netError) {
        return nativeGetMockUrlWithFailure(path, phase.ordinal(), netError);
    }

    private static native void nativeAddUrlInterceptors();

    private static native String nativeGetMockUrl(String path);

    private static native String nativeGetMockUrlWithFailure(String path,
            int phase, int netError);
}
