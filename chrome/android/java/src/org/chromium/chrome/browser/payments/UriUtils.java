// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.UrlConstants;

import java.net.URI;
import java.net.URISyntaxException;

import javax.annotation.Nullable;

/** URI utilities. */
public class UriUtils {
    /** The hostname used by the embedded test server in testing. */
    public static final String LOCALHOST_FOR_TEST = "127.0.0.1";

    /** The beginning part of the URL used by the embedded test server in testing. */
    private static final String LOCALHOST_URI_PREFIX_FOR_TEST = "http://127.0.0.1:";

    /** Whether HTTP localhost URIs should be allowed. Should be used only in testing. */
    private static boolean sAllowHttpForTest;

    /**
     * Checks whether the given <code>method</code> string has the correct format to be a URI
     * payment method name. Does not perform complete URI validation.
     *
     * @param method The payment method name to check.
     * @return Whether the method name has the correct format to be a URI payment method name.
     */
    public static boolean looksLikeUriMethod(String method) {
        return method.startsWith(UrlConstants.HTTPS_URL_PREFIX)
                || (sAllowHttpForTest && method.startsWith(LOCALHOST_URI_PREFIX_FOR_TEST));
    }

    /**
     * Parses the input <code>method</code> into a URI payment method name. Returns null for
     * invalid URI format or a relative URI.
     *
     * @param method The payment method name to parse.
     * @return The parsed URI payment method name or null if not valid.
     */
    @Nullable
    public static URI parseUriFromString(String method) {
        URI uri;
        try {
            // Don't use java.net.URL, because it performs a synchronous DNS lookup in the
            // constructor.
            uri = new URI(method);
        } catch (URISyntaxException e) {
            return null;
        }

        if (!uri.isAbsolute()) return null;

        assert UrlConstants.HTTPS_SCHEME.equals(uri.getScheme())
                || (sAllowHttpForTest && UrlConstants.HTTP_SCHEME.equals(uri.getScheme())
                           && LOCALHOST_FOR_TEST.equals(uri.getHost()));

        return uri;
    }

    /**
     * Returns the origin part of the given URI.
     *
     * @param uri The input URI for which the origin needs to be returned. Should not be null.
     * @return The origin of the input URI. Never null.
     */
    public static URI getOrigin(URI uri) {
        assert uri != null;

        // Tests use sub-directories to simulate different hosts, because the test web server runs
        // on a single localhost origin. Therefore, the "origin" in test is
        // https://127.0.0.1:12355/components/test/data/payments/bobpay.xyz instead of
        // https://127.0.0.1:12355.
        String originString = uri.resolve(sAllowHttpForTest ? "." : "/").toString();

        // Strip the trailing slash.
        if (!originString.isEmpty() && originString.charAt(originString.length() - 1) == '/') {
            originString = originString.substring(0, originString.length() - 1);
        }

        URI origin = parseUriFromString(originString);
        assert origin != null;

        return origin;
    }

    @VisibleForTesting
    public static void allowHttpForTest() {
        sAllowHttpForTest = true;
    }

    private UriUtils() {}
}