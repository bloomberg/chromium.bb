// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.chromium.base.JNINamespace;

/**
 * Wrapper class to start an in-process native test server, and get URLs
 * needed to talk to it.
 */
@JNINamespace("cronet")
public final class NativeTestServer {
    public static boolean startNativeTestServer(Context context) {
        return nativeStartNativeTestServer(
                TestFilesInstaller.getInstalledPath(context));
    }

    public static void shutdownNativeTestServer() {
        nativeShutdownNativeTestServer();
    }

    public static String getEchoBodyURL() {
        return nativeGetEchoBodyURL();
    }

    public static String getEchoHeaderURL(String header) {
        return nativeGetEchoHeaderURL(header);
    }

    public static String getEchoAllHeadersURL() {
        return nativeGetEchoAllHeadersURL();
    }

    public static String getEchoMethodURL() {
        return nativeGetEchoMethodURL();
    }

    public static String getRedirectToEchoBody() {
        return nativeGetRedirectToEchoBody();
    }

    public static String getFileURL(String filePath) {
        return nativeGetFileURL(filePath);
    }

    private static native boolean nativeStartNativeTestServer(String filePath);
    private static native void nativeShutdownNativeTestServer();
    private static native String nativeGetEchoBodyURL();
    private static native String nativeGetEchoHeaderURL(String header);
    private static native String nativeGetEchoAllHeadersURL();
    private static native String nativeGetEchoMethodURL();
    private static native String nativeGetRedirectToEchoBody();
    private static native String nativeGetFileURL(String filePath);
}
