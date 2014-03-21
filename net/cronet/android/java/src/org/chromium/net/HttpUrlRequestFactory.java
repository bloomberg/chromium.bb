// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.util.Log;

import java.lang.reflect.Constructor;
import java.nio.channels.WritableByteChannel;
import java.util.Map;

/**
 * A factory for {@link HttpUrlRequest}'s, which uses the best HTTP stack
 * available on the current platform.
 */
public abstract class HttpUrlRequestFactory {
    private static final Object sLock = new Object();

    private static final String TAG = "HttpUrlRequestFactory";

    private static final String CHROMIUM_URL_REQUEST_FACTORY =
            "org.chromium.net.ChromiumUrlRequestFactory";

    private static HttpUrlRequestFactory sFactory;

    private static HttpUrlRequestFactory getFactory(
            Context context) {
        synchronized (sLock) {
            if (sFactory == null) {
                try {
                    Class<? extends HttpUrlRequestFactory> factoryClass =
                            HttpUrlRequestFactory.class.getClassLoader().
                                    loadClass(CHROMIUM_URL_REQUEST_FACTORY).
                                    asSubclass(HttpUrlRequestFactory.class);
                    Constructor<? extends HttpUrlRequestFactory> constructor =
                            factoryClass.getConstructor(Context.class);
                    HttpUrlRequestFactory chromiumFactory =
                            constructor.newInstance(context);
                    if (chromiumFactory.isEnabled()) {
                        sFactory = chromiumFactory;
                    }
                } catch (ClassNotFoundException e) {
                    // Leave as null
                } catch (Exception e) {
                    throw new IllegalStateException(
                            "Cannot instantiate: " +
                            CHROMIUM_URL_REQUEST_FACTORY,
                            e);
                }
                if (sFactory == null) {
                    // Default to HttpUrlConnection-based networking.
                    sFactory = new HttpUrlConnectionUrlRequestFactory(context);
                }
                Log.i(TAG, "Using network stack: " + sFactory.getName());
            }
            return sFactory;
        }
    }

    /**
     * Creates a new request intended for full-response buffering.
     */
    public static HttpUrlRequest newRequest(Context context, String url,
            int requestPriority, Map<String, String> headers,
            HttpUrlRequestListener listener) {
        return getFactory(context).createRequest(url, requestPriority, headers,
                listener);
    }

    /**
     * Creates a new request intended for streaming the response.
     */
    public static HttpUrlRequest newRequest(Context context, String url,
            int requestPriority, Map<String, String> headers,
            WritableByteChannel channel, HttpUrlRequestListener listener) {
        return getFactory(context).createRequest(url, requestPriority, headers,
                channel, listener);
    }

    /**
     * Returns true if the factory is enabled.
     */
    protected abstract boolean isEnabled();

    /**
     * Returns a human-readable name of the factory.
     */
    protected abstract String getName();

    /**
     * Creates a new request intended for full-response buffering.
     */
    protected abstract HttpUrlRequest createRequest(String url,
            int requestPriority, Map<String, String> headers,
            HttpUrlRequestListener listener);

    /**
     * Creates a new request intended for streaming.
     */
    protected abstract HttpUrlRequest createRequest(String url,
            int requestPriority, Map<String, String> headers,
            WritableByteChannel channel, HttpUrlRequestListener listener);
}
