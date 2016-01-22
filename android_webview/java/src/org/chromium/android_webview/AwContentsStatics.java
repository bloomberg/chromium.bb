// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Implementations of various static methods, and also a home for static
 * data structures that are meant to be shared between all webviews.
 */
@JNINamespace("android_webview")
public class AwContentsStatics {

    private static ClientCertLookupTable sClientCertLookupTable;

    private static String sUnreachableWebDataUrl;

    private static boolean sRecordFullDocument = false;

    /**
     * Return the client certificate lookup table.
     */
    public static ClientCertLookupTable getClientCertLookupTable() {
        ThreadUtils.assertOnUiThread();
        if (sClientCertLookupTable == null) {
            sClientCertLookupTable = new ClientCertLookupTable();
        }
        return sClientCertLookupTable;
    }

    /**
     * Clear client cert lookup table. Should only be called from UI thread.
     */
    public static void clearClientCertPreferences(Runnable callback) {
        ThreadUtils.assertOnUiThread();
        getClientCertLookupTable().clear();
        nativeClearClientCertPreferences(callback);
    }

    @CalledByNative
    private static void clientCertificatesCleared(Runnable callback) {
        if (callback == null) return;
        callback.run();
    }

    /**
     * Set Data Reduction Proxy key for authentication.
     */
    public static void setDataReductionProxyKey(String key) {
        ThreadUtils.assertOnUiThread();
        nativeSetDataReductionProxyKey(key);
    }

    /*
     * Enable or disable data reduction proxy.
     */
    public static void setDataReductionProxyEnabled(boolean enabled) {
        ThreadUtils.assertOnUiThread();
        nativeSetDataReductionProxyEnabled(enabled);
    }

    public static String getUnreachableWebDataUrl() {
        // Note that this method may be called from both IO and UI threads,
        // but as it only retrieves a value of a constant from native, even if
        // two calls will be running at the same time, this should not cause
        // any harm.
        if (sUnreachableWebDataUrl == null) {
            sUnreachableWebDataUrl = nativeGetUnreachableWebDataUrl();
        }
        return sUnreachableWebDataUrl;
    }

    public static void setRecordFullDocument(boolean recordFullDocument) {
        sRecordFullDocument = recordFullDocument;
    }

    /* package */ static boolean getRecordFullDocument() {
        return sRecordFullDocument;
    }

    public static void setLegacyCacheRemovalDelayForTest(long timeoutMs) {
        nativeSetLegacyCacheRemovalDelayForTest(timeoutMs);
    }

    public static String getProductVersion() {
        return nativeGetProductVersion();
    }

    public static void setServiceWorkerIoThreadClient(AwContentsIoThreadClient ioThreadClient,
            AwBrowserContext browserContext) {
        nativeSetServiceWorkerIoThreadClient(ioThreadClient, browserContext);
    }

    //--------------------------------------------------------------------------------------------
    //  Native methods
    //--------------------------------------------------------------------------------------------
    private static native void nativeClearClientCertPreferences(Runnable callback);
    private static native void nativeSetDataReductionProxyKey(String key);
    private static native void nativeSetDataReductionProxyEnabled(boolean enabled);
    private static native String nativeGetUnreachableWebDataUrl();
    private static native void nativeSetLegacyCacheRemovalDelayForTest(long timeoutMs);
    private static native String nativeGetProductVersion();
    private static native void nativeSetServiceWorkerIoThreadClient(
            AwContentsIoThreadClient ioThreadClient, AwBrowserContext browserContext);
}
