// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.ParseException;
import android.util.Log;

import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;

import java.util.concurrent.Callable;

/**
 * CookieManager manages cookies according to RFC2109 spec.
 *
 * Methods in this class are thread safe.
 */
@JNINamespace("android_webview")
public final class CookieManager {
    private static final String LOGTAG = "CookieManager";

    /**
     * Control whether cookie is enabled or disabled
     * @param accept TRUE if accept cookie
     */
    public synchronized void setAcceptCookie(boolean accept) {
        final boolean finalAccept = accept;
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                nativeSetAcceptCookie(finalAccept);
            }
        });
    }

    private final Callable<Boolean> acceptCookieCallable = new Callable<Boolean>() {
        @Override
        public Boolean call() throws Exception {
            return nativeAcceptCookie();
        }
    };

    /**
     * Return whether cookie is enabled
     * @return TRUE if accept cookie
     */
    public synchronized boolean acceptCookie() {
        return ThreadUtils.runOnUiThreadBlockingNoException(acceptCookieCallable);
    }

    /**
     * Set cookie for a given url. The old cookie with same host/path/name will
     * be removed. The new cookie will be added if it is not expired or it does
     * not have expiration which implies it is session cookie.
     * @param url The url which cookie is set for
     * @param value The value for set-cookie: in http response header
     */
    public void setCookie(final String url, final String value) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                nativeSetCookie(url, value);
            }
        });
    }

    /**
     * Get cookie(s) for a given url so that it can be set to "cookie:" in http
     * request header.
     * @param url The url needs cookie
     * @return The cookies in the format of NAME=VALUE [; NAME=VALUE]
     */
    public String getCookie(final String url) {
        String cookie = ThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return nativeGetCookie(url.toString());
            }
        });
        // Return null if the string is empty to match legacy behavior
        return cookie == null || cookie.trim().isEmpty() ? null : cookie;
    }

    private final Runnable removeSessionCookieRunnable = new Runnable() {
        @Override
        public void run() {
            nativeRemoveSessionCookie();
        }
    };

    /**
     * Remove all session cookies, which are cookies without expiration date
     */
    public void removeSessionCookie() {
        ThreadUtils.runOnUiThread(removeSessionCookieRunnable);
    }

    private final Runnable removeAllCookieRunnable = new Runnable() {
        @Override
        public void run() {
            nativeRemoveAllCookie();
        }
    };

    /**
     * Remove all cookies
     */
    public void removeAllCookie() {
        ThreadUtils.runOnUiThread(removeAllCookieRunnable);
    }

    private final Callable<Boolean> hasCookiesCallable = new Callable<Boolean>() {
        @Override
        public Boolean call() throws Exception {
            return nativeHasCookies();
        }
    };

    /**
     *  Return true if there are stored cookies.
     */
    public synchronized boolean hasCookies() {
        return ThreadUtils.runOnUiThreadBlockingNoException(hasCookiesCallable);
    }

    private final Runnable removeExpiredCookieRunnable = new Runnable() {
        @Override
        public void run() {
            nativeRemoveExpiredCookie();
        }
    };

    /**
     * Remove all expired cookies
     */
    public void removeExpiredCookie() {
        ThreadUtils.runOnUiThread(removeExpiredCookieRunnable);
    }

    private static final Callable<Boolean> allowFileSchemeCookiesCallable =
            new Callable<Boolean>() {
        @Override
        public Boolean call() throws Exception {
            return nativeAllowFileSchemeCookies();
        }
    };

    /**
     * Whether cookies are accepted for file scheme URLs.
     */
    public static boolean allowFileSchemeCookies() {
        return ThreadUtils.runOnUiThreadBlockingNoException(allowFileSchemeCookiesCallable);
    }

    /**
     * Sets whether cookies are accepted for file scheme URLs.
     *
     * Use of cookies with file scheme URLs is potentially insecure. Do not use this feature unless
     * you can be sure that no unintentional sharing of cookie data can take place.
     * <p>
     * Note that calls to this method will have no effect if made after a WebView or CookieManager
     * instance has been created.
     */
    public static void setAcceptFileSchemeCookies(boolean accept) {
        final boolean finalAccept = accept;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                nativeSetAcceptFileSchemeCookies(finalAccept);
            }
        });
    }

    private native void nativeSetAcceptCookie(boolean accept);
    private native boolean nativeAcceptCookie();

    private native void nativeSetCookie(String url, String value);
    private native String nativeGetCookie(String url);

    private native void nativeRemoveSessionCookie();
    private native void nativeRemoveAllCookie();
    private native void nativeRemoveExpiredCookie();

    private native boolean nativeHasCookies();

    static native boolean nativeAllowFileSchemeCookies();
    static native void nativeSetAcceptFileSchemeCookies(boolean accept);
}
