// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import org.chromium.base.JNINamespace;

/**
 * Wrapper for the dom_distiller::url_utils.
 */
@JNINamespace("dom_distiller::url_utils::android")
public final class DomDistillerUrlUtils {

    private DomDistillerUrlUtils() {
    }

    /**
     * Returns the URL for viewing distilled content for a URL.
     *
     * @param scheme The scheme for the DOM Distiller source.
     * @param url The URL to distill.
     * @return the URL to load to get the distilled version of a page.
     */
    public static String getDistillerViewUrlFromUrl(String scheme, String url) {
        return nativeGetDistillerViewUrlFromUrl(scheme, url);
    }

    /**
     * Returns the original URL of a distillation given the viewer URL.
     *
     * @param url The current viewer URL.
     * @return the URL of the original page.
     */
    public static String getOriginalUrlFromDistillerUrl(String url) {
        return nativeGetOriginalUrlFromDistillerUrl(url);
    }

    public static boolean isUrlReportable(String scheme, String url) {
        return nativeIsUrlReportable(scheme, url);
    }

    private static native String nativeGetDistillerViewUrlFromUrl(String scheme, String url);
    private static native String nativeGetOriginalUrlFromDistillerUrl(String viewerUrl);
    private static native boolean nativeIsUrlReportable(String scheme, String url);
}
