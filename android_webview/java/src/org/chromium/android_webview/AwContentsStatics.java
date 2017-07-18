// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.TargetApi;
import android.content.Context;
import android.webkit.ValueCallback;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.lang.reflect.Method;

/**
 * Implementations of various static methods, and also a home for static
 * data structures that are meant to be shared between all webviews.
 */
@JNINamespace("android_webview")
public class AwContentsStatics {

    private static ClientCertLookupTable sClientCertLookupTable;

    private static String sUnreachableWebDataUrl;

    private static boolean sRecordFullDocument;

    private static final String sSafeBrowsingWarmUpHelper =
            "com.android.webview.chromium.SafeBrowsingWarmUpHelper";

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

    // Can be called from any thread.
    public static boolean getSafeBrowsingEnabledByManifest() {
        return nativeGetSafeBrowsingEnabledByManifest();
    }

    public static void setSafeBrowsingEnabledByManifest(boolean enable) {
        nativeSetSafeBrowsingEnabledByManifest(enable);
    }

    // TODO(ntfschr): remove this when downstream no longer depends on it
    public static boolean getSafeBrowsingEnabled() {
        return getSafeBrowsingEnabledByManifest();
    }

    // TODO(ntfschr): remove this when downstream no longer depends on it
    public static void setSafeBrowsingEnabled(boolean enable) {
        setSafeBrowsingEnabledByManifest(enable);
    }

    public static void setSafeBrowsingWhiteList(String[] urls) {
        nativeSetSafeBrowsingWhiteList(urls);
    }

    @SuppressWarnings("unchecked")
    @TargetApi(19)
    public static void initSafeBrowsing(Context context, ValueCallback<Boolean> callback) {
        if (callback == null) {
            callback = new ValueCallback<Boolean>() {
                @Override
                public void onReceiveValue(Boolean b) {}
            };
        }

        try {
            Class cls = Class.forName(sSafeBrowsingWarmUpHelper);
            Method m =
                    cls.getDeclaredMethod("warmUpSafeBrowsing", Context.class, ValueCallback.class);
            m.invoke(null, context, callback);
        } catch (ReflectiveOperationException e) {
            callback.onReceiveValue(false);
        }
    }

    @SuppressWarnings("unchecked")
    @TargetApi(19)
    public static void shutdownSafeBrowsing() {
        try {
            Class cls = Class.forName(sSafeBrowsingWarmUpHelper);
            Method m = cls.getDeclaredMethod("coolDownSafeBrowsing");
            m.invoke(null);
        } catch (ReflectiveOperationException e) {
            // This is not an error; it just means this device doesn't have specialized services.
        }
    }

    public static void setCheckClearTextPermitted(boolean permitted) {
        nativeSetCheckClearTextPermitted(permitted);
    }

    /**
     * Return the first substring consisting of the address of a physical location.
     * @see {@link android.webkit.WebView#findAddress(String)}
     *
     * @param addr The string to search for addresses.
     * @return the address, or if no address is found, return null.
     */
    public static String findAddress(String addr) {
        if (addr == null) {
            throw new NullPointerException("addr is null");
        }
        String result = nativeFindAddress(addr);
        return result == null || result.isEmpty() ? null : result;
    }

    //--------------------------------------------------------------------------------------------
    //  Native methods
    //--------------------------------------------------------------------------------------------
    private static native void nativeClearClientCertPreferences(Runnable callback);
    private static native String nativeGetUnreachableWebDataUrl();
    private static native void nativeSetLegacyCacheRemovalDelayForTest(long timeoutMs);
    private static native String nativeGetProductVersion();
    private static native void nativeSetServiceWorkerIoThreadClient(
            AwContentsIoThreadClient ioThreadClient, AwBrowserContext browserContext);
    private static native boolean nativeGetSafeBrowsingEnabledByManifest();
    private static native void nativeSetSafeBrowsingEnabledByManifest(boolean enable);
    private static native void nativeSetSafeBrowsingWhiteList(String[] urls);
    private static native void nativeSetCheckClearTextPermitted(boolean permitted);
    private static native String nativeFindAddress(String addr);
}
