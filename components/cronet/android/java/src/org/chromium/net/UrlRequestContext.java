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
     * Creates an UrlRequest object. All UrlRequest functions must
     * be called on the Executor's thread, and all callbacks will be called
     * on the Executor's thread as well. Executor must not run tasks on the
     * current thread to prevent network jank and exception during shutdown.
     *
     * createRequest itself may be called on any thread.
     * @param url URL for the request.
     * @param listener Callback interface that gets called on different events.
     * @param executor Executor on which all callbacks will be called.
     * @return new request.
     */
    public abstract UrlRequest createRequest(String url,
            UrlRequestListener listener, Executor executor);

    /**
     * @return true if the context is enabled.
     */
    public abstract boolean isEnabled();

    /**
     * @return a human-readable version string of the context.
     */
    public abstract String getVersionString();

    /**
     * Shuts down the UrlRequestContext if there are no active requests,
     * otherwise throws an exception.
     *
     * Cannot be called on network thread - the thread  Cronet calls into
     * Executor on (which is different from the thread the Executor invokes
     * callbacks on).  May block until all the Context's resources have been
     * cleaned up.
     */
    public abstract void shutdown();

    /**
     * Starts NetLog logging to a file. The NetLog log level used is
     * LOG_ALL_BUT_BYTES.
     * @param fileName The complete file path. It must not be empty. If file
     *            exists, it is truncated before starting. If actively logging,
     *            this method is ignored.
     */
    public abstract void startNetLogToFile(String fileName);

    /**
     * Stops NetLog logging and flushes file to disk. If a logging session is
     * not in progress, this call is ignored.
     */
    public abstract void stopNetLog();

    /**
     * Create context with given config. If config.legacyMode is true, or
     * native library is not available, then creates HttpUrlConnection-based
     * context.
     * @param context application context.
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
