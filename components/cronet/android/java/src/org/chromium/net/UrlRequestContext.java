// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.util.Log;

import java.lang.reflect.Constructor;
import java.util.concurrent.Executor;

/**
 * A context for {@link UrlRequest}'s, which uses the best HTTP stack
 * available on the current platform.
 */
public abstract class UrlRequestContext {
    private static final String TAG = "UrlRequestFactory";
    private static final String CRONET_URL_REQUEST_CONTEXT =
            "org.chromium.net.CronetUrlRequestContext";

    /**
     * Creates a {@link UrlRequest} object. All callbacks will
     * be called on {@code executor}'s thread. {@code executor} must not run
     * tasks on the current thread to prevent blocking networking operations
     * and causing exceptions during shutdown. Request is given medium priority,
     * see {@link UrlRequest#REQUEST_PRIORITY_MEDIUM}. To specify other
     * priorities see {@link #createRequest(String, UrlRequestListener,
     * Executor, int priority)}.
     *
     * @param url {@link java.net.URL} for the request.
     * @param listener callback interface that gets called on different events.
     * @param executor {@link Executor} on which all callbacks will be called.
     * @return new request.
     */
    public abstract UrlRequest createRequest(String url,
            UrlRequestListener listener, Executor executor);

    /**
     * Creates a {@link UrlRequest} object. All callbacks will
     * be called on {@code executor}'s thread. {@code executor} must not run
     * tasks on the current thread to prevent blocking networking operations
     * and causing exceptions during shutdown.
     *
     * @param url {@link java.net.URL} for the request.
     * @param listener callback interface that gets called on different events.
     * @param executor {@link Executor} on which all callbacks will be called.
     * @param priority priority of the request which should be one of the
     *         {@link UrlRequest#REQUEST_PRIORITY_IDLE REQUEST_PRIORITY_*}
     *         values.
     * @return new request.
     */
    public abstract UrlRequest createRequest(String url,
            UrlRequestListener listener, Executor executor, int priority);

    /**
     * @return {@code true} if the context is enabled.
     */
    abstract boolean isEnabled();

    /**
     * @return a human-readable version string of the context.
     */
    public abstract String getVersionString();

    /**
     * Shuts down the {@link UrlRequestContext} if there are no active requests,
     * otherwise throws an exception.
     *
     * Cannot be called on network thread - the thread Cronet calls into
     * Executor on (which is different from the thread the Executor invokes
     * callbacks on). May block until all the {@code UrlRequestContext}'s
     * resources have been cleaned up.
     */
    public abstract void shutdown();

    /**
     * Starts NetLog logging to a file. The NetLog is useful for debugging.
     * The file can be viewed using a Chrome browser navigated to
     * chrome://net-internals/#import
     * @param fileName the complete file path. It must not be empty. If the file
     *            exists, it is truncated before starting. If actively logging,
     *            this method is ignored.
     * @param logAll {@code true} to include basic events, user cookies,
     *            credentials and all transferred bytes in the log.
     *            {@code false} to just include basic events.
     */
    public abstract void startNetLogToFile(String fileName, boolean logAll);

    /**
     * Stops NetLog logging and flushes file to disk. If a logging session is
     * not in progress, this call is ignored.
     */
    public abstract void stopNetLog();

    /**
     * Creates a {@link UrlRequestContext} with the given
     * {@link UrlRequestContextConfig}.
     * @param context Android {@link Context}.
     * @param config context configuration.
     */
    public static UrlRequestContext createContext(Context context,
            UrlRequestContextConfig config) {
        UrlRequestContext urlRequestContext = null;
        if (config.userAgent().isEmpty()) {
            config.setUserAgent(UserAgent.from(context));
        }
        if (!config.legacyMode()) {
            urlRequestContext = createCronetContext(context, config);
        }
        if (urlRequestContext == null) {
            // TODO(mef): Fallback to stub implementation. Once stub
            // implementation is available merge with createCronetFactory.
            urlRequestContext = createCronetContext(context, config);
        }
        Log.i(TAG, "Using network stack: "
                + urlRequestContext.getVersionString());
        return urlRequestContext;
    }

    private static UrlRequestContext createCronetContext(Context context,
            UrlRequestContextConfig config) {
        UrlRequestContext urlRequestContext = null;
        try {
            Class<? extends UrlRequestContext> contextClass =
                    UrlRequestContext.class.getClassLoader()
                            .loadClass(CRONET_URL_REQUEST_CONTEXT)
                            .asSubclass(UrlRequestContext.class);
            Constructor<? extends UrlRequestContext> constructor =
                    contextClass.getConstructor(
                            Context.class, UrlRequestContextConfig.class);
            UrlRequestContext cronetContext =
                    constructor.newInstance(context, config);
            if (cronetContext.isEnabled()) {
                urlRequestContext = cronetContext;
            }
        } catch (ClassNotFoundException e) {
            // Leave as null.
        } catch (Exception e) {
            throw new IllegalStateException(
                    "Cannot instantiate: " + CRONET_URL_REQUEST_CONTEXT,
                    e);
        }
        return urlRequestContext;
    }
}
