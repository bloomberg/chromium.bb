// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.chromium.base.JNINamespace;

/**
 * Helper class to set up url interceptors for testing purposes.
 */
@JNINamespace("cronet")
public final class MockUrlRequestJobFactory {
    public static final String SUCCESS_URL =
            "http://mock.http/success.txt";
    public static final String REDIRECT_URL =
            "http://mock.http/redirect.html";
    public static final String MULTI_REDIRECT_URL =
            "http://mock.http/multiredirect.html";
    public static final String NOTFOUND_URL =
            "http://mock.http/notfound.html";
    public static final String FAILED_URL =
            "http://mock.failed.request/-2";

    enum FailurePhase {
        START,
        READ_ASYNC,
        READ_SYNC,
    };

    /**
     * Constructs a MockUrlRequestJobFactory and sets up mock environment.
     * @param context application context.
     */
    public MockUrlRequestJobFactory(Context context) {
        if (!TestFilesInstaller.areFilesInstalled(context)) {
            throw new IllegalStateException("test files not installed.");
        }
        nativeAddUrlInterceptors(TestFilesInstaller.getInstalledPath(context));
    }

    /**
     * Constructs a mock URL.
     *
     * @param path path to a mock file.
     */
    public String getMockUrl(String path) {
        return nativeGetMockUrl(path);
    }

    /**
     * Constructs a mock URL that hangs or fails at certain phase.
     *
     * @param path path to a mock file.
     * @param phase at which request fails.
     * @param netError reported by UrlRequestJob. Passing -1, results in hang.
     */
    public String getMockUrlWithFailure(String path, FailurePhase phase,
            int netError) {
        return nativeGetMockUrlWithFailure(path, phase.ordinal(), netError);
    }

    /**
     * Constructs a mock URL that synchronously responds with data repeated many
     * times.
     *
     * @param data to return in response.
     * @param dataRepeatCount number of times to repeat the data.
     */
    public String getMockUrlForData(String data, int dataRepeatCount) {
        return nativeGetMockUrlForData(data, dataRepeatCount);
    }

    private static native void nativeAddUrlInterceptors(String installedPath);

    private static native String nativeGetMockUrl(String path);

    private static native String nativeGetMockUrlWithFailure(String path,
            int phase, int netError);

    private static native String nativeGetMockUrlForData(String data,
            int dataRepeatCount);
}
