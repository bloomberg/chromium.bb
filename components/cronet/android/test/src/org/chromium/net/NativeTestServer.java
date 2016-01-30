// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.ConditionVariable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Wrapper class to start an in-process native test server, and get URLs
 * needed to talk to it.
 */
@JNINamespace("cronet")
public final class NativeTestServer {
    private static final ConditionVariable sHostResolverBlock = new ConditionVariable();

    // This variable contains the response body of a request to getSuccessURL().
    public static final String SUCCESS_BODY = "this is a text file\n";

    public static boolean startNativeTestServer(Context context) {
        TestFilesInstaller.installIfNeeded(context);
        return nativeStartNativeTestServer(
                TestFilesInstaller.getInstalledPath(context));
    }

    public static void shutdownNativeTestServer() {
        nativeShutdownNativeTestServer();
    }

    /**
     * Registers customized DNS mapping for {@link NativeTestServer}.
     * @param contextAdapter native context adapter object that this
     *             mapping should apply to.
     * @param isLegacyAPI {@code true} if this context adapter is a part of the
     *             old API.
     */
    public static void registerHostResolverProc(long contextAdapter, boolean isLegacyAPI) {
        nativeRegisterHostResolverProc(contextAdapter, isLegacyAPI);
        sHostResolverBlock.block();
        sHostResolverBlock.close();
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

    public static String getSdchURL() {
        return nativeGetSdchURL();
    }

    // The following URLs will make NativeTestServer serve a response based on
    // the contents of the corresponding file and its mock-http-headers file.

    public static String getSuccessURL() {
        return nativeGetFileURL("/success.txt");
    }

    public static String getRedirectURL() {
        return nativeGetFileURL("/redirect.html");
    }

    public static String getMultiRedirectURL() {
        return nativeGetFileURL("/multiredirect.html");
    }

    public static String getNotFoundURL() {
        return nativeGetFileURL("/notfound.html");
    }

    public static String getHostPort() {
        return nativeGetHostPort();
    }

    public static boolean isDataReductionProxySupported() {
        return nativeIsDataReductionProxySupported();
    }

    @CalledByNative
    private static void onHostResolverProcRegistered() {
        sHostResolverBlock.open();
    }

    private static native boolean nativeStartNativeTestServer(String filePath);
    private static native void nativeShutdownNativeTestServer();
    private static native void nativeRegisterHostResolverProc(
            long contextAdapter, boolean isLegacyAPI);
    private static native String nativeGetEchoBodyURL();
    private static native String nativeGetEchoHeaderURL(String header);
    private static native String nativeGetEchoAllHeadersURL();
    private static native String nativeGetEchoMethodURL();
    private static native String nativeGetRedirectToEchoBody();
    private static native String nativeGetFileURL(String filePath);
    private static native String nativeGetSdchURL();
    private static native String nativeGetHostPort();
    private static native boolean nativeIsDataReductionProxySupported();
}
