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
    private static final String TAG = "HttpUrlRequestFactory";

    private static final String CHROMIUM_URL_REQUEST_FACTORY =
            "org.chromium.net.ChromiumUrlRequestFactory";

    public static HttpUrlRequestFactory createFactory(
            Context context, HttpUrlRequestFactoryConfig config) {
        HttpUrlRequestFactory factory = null;
        if (!config.legacyMode()) {
            factory = createChromiumFactory(context, config);
        }
        if (factory == null) {
            // Default to HttpUrlConnection-based networking.
            factory = new HttpUrlConnectionUrlRequestFactory(context, config);
        }
        Log.i(TAG, "Using network stack: " + factory.getName());
        return factory;
    }

    /**
     * Returns true if the factory is enabled.
     */
    public abstract boolean isEnabled();

    /**
     * Returns a human-readable name of the factory.
     */
    public abstract String getName();

    /**
     * Creates a new request intended for full-response buffering.
     */
    public abstract HttpUrlRequest createRequest(String url,
            int requestPriority, Map<String, String> headers,
            HttpUrlRequestListener listener);

    /**
     * Creates a new request intended for streaming.
     */
    public abstract HttpUrlRequest createRequest(String url,
            int requestPriority, Map<String, String> headers,
            WritableByteChannel channel, HttpUrlRequestListener listener);

    private static HttpUrlRequestFactory createChromiumFactory(
            Context context, HttpUrlRequestFactoryConfig config) {
        HttpUrlRequestFactory factory = null;
        try {
            Class<? extends HttpUrlRequestFactory> factoryClass =
                    HttpUrlRequestFactory.class.getClassLoader().
                            loadClass(CHROMIUM_URL_REQUEST_FACTORY).
                            asSubclass(HttpUrlRequestFactory.class);
            Constructor<? extends HttpUrlRequestFactory> constructor =
                    factoryClass.getConstructor(
                            Context.class, HttpUrlRequestFactoryConfig.class);
            HttpUrlRequestFactory chromiumFactory =
                    constructor.newInstance(context, config);
            if (chromiumFactory.isEnabled()) {
                factory = chromiumFactory;
            }
        } catch (ClassNotFoundException e) {
            // Leave as null
        } catch (Exception e) {
            throw new IllegalStateException(
                    "Cannot instantiate: " +
                    CHROMIUM_URL_REQUEST_FACTORY,
                    e);
        }
        return factory;
    }
}
