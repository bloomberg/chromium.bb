// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.lang.reflect.Constructor;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.util.concurrent.Executor;

/**
 * An engine to process {@link UrlRequest}s, which uses the best HTTP stack
 * available on the current platform.
 */
public abstract class CronetEngine {
    /**
     * A builder for {@link CronetEngine}s, which allows runtime configuration of
     * {@code CronetEngine}. Configuration options are set on the builder and
     * then {@link #build} is called to create the {@code CronetEngine}.
     */
    public static class Builder {
        private final JSONObject mConfig;
        private final Context mContext;

        /**
         * Default config enables SPDY, disables QUIC, SDCH and HTTP cache.
         * @param context Android {@link Context} for engine to use.
         */
        public Builder(Context context) {
            mConfig = new JSONObject();
            mContext = context;
            enableLegacyMode(false);
            enableQUIC(false);
            enableHTTP2(true);
            enableSDCH(false);
            enableHttpCache(HTTP_CACHE_DISABLED, 0);
        }

        /**
         * Creates a config from a JSON string, which was serialized using
         * {@link #toString}.
         *
         * @param context Android {@link Context} for engine to use.
         * @param json JSON string of configuration parameters, which was
         *         serialized using {@link #toString}.
         */
        public Builder(Context context, String json) throws JSONException {
            mConfig = new JSONObject(json);
            mContext = context;
        }

        /**
         * Constructs a User-Agent string including Cronet version, and
         * application name and version.
         *
         * @return User-Agent string.
         */
        public String getDefaultUserAgent() {
            return UserAgent.from(mContext);
        }

        /**
         * Overrides the User-Agent header for all requests.
         * @return the builder to facilitate chaining.
         */
        public Builder setUserAgent(String userAgent) {
            return putString(CronetEngineBuilderList.USER_AGENT, userAgent);
        }

        String getUserAgent() {
            return mConfig.optString(CronetEngineBuilderList.USER_AGENT);
        }

        /**
         * Sets directory for HTTP Cache and Cookie Storage. The directory must
         * exist.
         * <p>
         * <b>NOTE:</b> Do not use the same storage directory with more than one
         * {@code CronetEngine} at a time. Access to the storage directory does
         * not support concurrent access by multiple {@code CronetEngine}s.
         *
         * @param value path to existing directory.
         * @return the builder to facilitate chaining.
         */
        public Builder setStoragePath(String value) {
            if (!new File(value).isDirectory()) {
                throw new IllegalArgumentException(
                        "Storage path must be set to existing directory");
            }

            return putString(CronetEngineBuilderList.STORAGE_PATH, value);
        }

        String storagePath() {
            return mConfig.optString(CronetEngineBuilderList.STORAGE_PATH);
        }

        /**
         * Sets whether falling back to implementation based on system's
         * {@link java.net.HttpURLConnection} implementation is enabled.
         * Defaults to disabled.
         * @return the builder to facilitate chaining.
         * @deprecated Not supported by the new API.
         */
        @Deprecated
        public Builder enableLegacyMode(boolean value) {
            return putBoolean(CronetEngineBuilderList.ENABLE_LEGACY_MODE, value);
        }

        boolean legacyMode() {
            return mConfig.optBoolean(CronetEngineBuilderList.ENABLE_LEGACY_MODE);
        }

        /**
         * Overrides the name of the native library backing Cronet.
         * @return the builder to facilitate chaining.
         */
        Builder setLibraryName(String libName) {
            return putString(CronetEngineBuilderList.NATIVE_LIBRARY_NAME, libName);
        }

        String libraryName() {
            return mConfig.optString(CronetEngineBuilderList.NATIVE_LIBRARY_NAME, "cronet");
        }

        /**
         * Sets whether <a href="https://www.chromium.org/quic">QUIC</a> protocol
         * is enabled. Defaults to disabled.
         * @return the builder to facilitate chaining.
         */
        public Builder enableQUIC(boolean value) {
            return putBoolean(CronetEngineBuilderList.ENABLE_QUIC, value);
        }

        /**
         * Sets whether <a href="https://tools.ietf.org/html/rfc7540">HTTP/2</a>
         * protocol is enabled. Defaults to enabled.
         * @return the builder to facilitate chaining.
         */
        public Builder enableHTTP2(boolean value) {
            return putBoolean(CronetEngineBuilderList.ENABLE_SPDY, value);
        }

        /**
         * Sets whether
         * <a
         * href="https://lists.w3.org/Archives/Public/ietf-http-wg/2008JulSep/att-0441/Shared_Dictionary_Compression_over_HTTP.pdf">
         * SDCH</a> compression is enabled. Defaults to disabled.
         * @return the builder to facilitate chaining.
         */
        public Builder enableSDCH(boolean value) {
            return putBoolean(CronetEngineBuilderList.ENABLE_SDCH, value);
        }

        /**
         * Enables
         * <a href="https://developer.chrome.com/multidevice/data-compression">Data
         * Reduction Proxy</a>. Defaults to disabled.
         * @param key key to use when authenticating with the proxy.
         * @return the builder to facilitate chaining.
         */
        public Builder enableDataReductionProxy(String key) {
            return (putString(CronetEngineBuilderList.DATA_REDUCTION_PROXY_KEY, key));
        }

        /**
         * Overrides
         * <a href="https://developer.chrome.com/multidevice/data-compression">
         * Data Reduction Proxy</a> configuration parameters with a primary
         * proxy name, fallback proxy name, and a secure proxy check URL. Proxies
         * are specified as [scheme://]host[:port]. Used for testing.
         * @param primaryProxy the primary data reduction proxy to use.
         * @param fallbackProxy a fallback data reduction proxy to use.
         * @param secureProxyCheckUrl a URL to fetch to determine if using a secure
         * proxy is allowed.
         * @return the builder to facilitate chaining.
         * @hide
         * @deprecated Marked as deprecated because @hide doesn't properly hide but
         *         javadocs are built with nodeprecated="yes".
         */
        public Builder setDataReductionProxyOptions(
                String primaryProxy, String fallbackProxy, String secureProxyCheckUrl) {
            if (primaryProxy.isEmpty() || fallbackProxy.isEmpty()
                    || secureProxyCheckUrl.isEmpty()) {
                throw new IllegalArgumentException(
                        "Primary and fallback proxies and check url must be set");
            }
            putString(CronetEngineBuilderList.DATA_REDUCTION_PRIMARY_PROXY, primaryProxy);
            putString(CronetEngineBuilderList.DATA_REDUCTION_FALLBACK_PROXY, fallbackProxy);
            putString(CronetEngineBuilderList.DATA_REDUCTION_SECURE_PROXY_CHECK_URL,
                    secureProxyCheckUrl);
            return this;
        }

        /**
         * Setting to disable HTTP cache. Some data may still be temporarily stored in memory.
         * Passed to {@link #enableHttpCache}.
         */
        public static final int HTTP_CACHE_DISABLED = 0;

        /**
         * Setting to enable in-memory HTTP cache, including HTTP data.
         * Passed to {@link #enableHttpCache}.
         */
        public static final int HTTP_CACHE_IN_MEMORY = 1;

        /**
         * Setting to enable on-disk cache, excluding HTTP data.
         * {@link #setStoragePath} must be called prior to passing this constant to
         * {@link #enableHttpCache}.
         */
        public static final int HTTP_CACHE_DISK_NO_HTTP = 2;

        /**
         * Setting to enable on-disk cache, including HTTP data.
         * {@link #setStoragePath} must be called prior to passing this constant to
         * {@link #enableHttpCache}.
         */
        public static final int HTTP_CACHE_DISK = 3;

        /**
         * Enables or disables caching of HTTP data and other information like QUIC
         * server information.
         * @param cacheMode control location and type of cached data.
         * @param maxSize maximum size in bytes used to cache data (advisory and maybe
         * exceeded at times).
         * @return the builder to facilitate chaining.
         */
        public Builder enableHttpCache(int cacheMode, long maxSize) {
            if (cacheMode == HTTP_CACHE_DISK || cacheMode == HTTP_CACHE_DISK_NO_HTTP) {
                if (storagePath().isEmpty()) {
                    throw new IllegalArgumentException("Storage path must be set");
                }
            } else {
                if (!storagePath().isEmpty()) {
                    throw new IllegalArgumentException("Storage path must be empty");
                }
            }
            putBoolean(CronetEngineBuilderList.LOAD_DISABLE_CACHE,
                    cacheMode == HTTP_CACHE_DISABLED || cacheMode == HTTP_CACHE_DISK_NO_HTTP);
            putLong(CronetEngineBuilderList.HTTP_CACHE_MAX_SIZE, maxSize);

            switch (cacheMode) {
                case HTTP_CACHE_DISABLED:
                    return putString(CronetEngineBuilderList.HTTP_CACHE,
                            CronetEngineBuilderList.HTTP_CACHE_DISABLED);
                case HTTP_CACHE_DISK_NO_HTTP:
                case HTTP_CACHE_DISK:
                    return putString(CronetEngineBuilderList.HTTP_CACHE,
                            CronetEngineBuilderList.HTTP_CACHE_DISK);

                case HTTP_CACHE_IN_MEMORY:
                    return putString(CronetEngineBuilderList.HTTP_CACHE,
                            CronetEngineBuilderList.HTTP_CACHE_MEMORY);
            }
            return this;
        }

        /**
         * Adds hint that {@code host} supports QUIC.
         * Note that {@link #enableHttpCache enableHttpCache}
         * ({@link HttpCache#DISK DISK}) is needed to take advantage of 0-RTT
         * connection establishment between sessions.
         *
         * @param host hostname of the server that supports QUIC.
         * @param port host of the server that supports QUIC.
         * @param alternatePort alternate port to use for QUIC.
         * @return the builder to facilitate chaining.
         */
        public Builder addQuicHint(String host, int port, int alternatePort) {
            if (host.contains("/")) {
                throw new IllegalArgumentException("Illegal QUIC Hint Host: " + host);
            }
            try {
                JSONArray quicHints = mConfig.optJSONArray(CronetEngineBuilderList.QUIC_HINTS);
                if (quicHints == null) {
                    quicHints = new JSONArray();
                    mConfig.put(CronetEngineBuilderList.QUIC_HINTS, quicHints);
                }

                JSONObject hint = new JSONObject();
                hint.put(CronetEngineBuilderList.QUIC_HINT_HOST, host);
                hint.put(CronetEngineBuilderList.QUIC_HINT_PORT, port);
                hint.put(CronetEngineBuilderList.QUIC_HINT_ALT_PORT, alternatePort);
                quicHints.put(hint);
            } catch (JSONException e) {
                // Intentionally do nothing.
            }
            return this;
        }

        /**
         * Sets experimental QUIC connection options, overwriting any pre-existing
         * options. List of options is subject to change.
         *
         * @param quicConnectionOptions comma-separated QUIC options (for example
         * "PACE,IW10") to use if QUIC is enabled.
         * @return the builder to facilitate chaining.
         */
        public Builder setExperimentalQuicConnectionOptions(String quicConnectionOptions) {
            return putString(CronetEngineBuilderList.QUIC_OPTIONS, quicConnectionOptions);
        }

        /**
         * Get JSON string representation of the builder.
         */
        @Override
        public String toString() {
            return mConfig.toString();
        }

        /**
         * Returns {@link Context} for builder.
         *
         * @return {@link Context} for builder.
         */
        Context getContext() {
            return mContext;
        }

        /**
         * Sets a boolean value in the config. Returns a reference to the same
         * config object, so you can chain put calls together.
         * @return the builder to facilitate chaining.
         */
        private Builder putBoolean(String key, boolean value) {
            try {
                mConfig.put(key, value);
            } catch (JSONException e) {
                // Intentionally do nothing.
            }
            return this;
        }

        /**
         * Sets a long value in the config. Returns a reference to the same
         * config object, so you can chain put calls together.
         * @return the builder to facilitate chaining.
         */
        private Builder putLong(String key, long value) {
            try {
                mConfig.put(key, value);
            } catch (JSONException e) {
                // Intentionally do nothing.
            }
            return this;
        }

        /**
         * Sets a string value in the config. Returns a reference to the same
         * config object, so you can chain put calls together.
         * @return the builder to facilitate chaining.
         */
        private Builder putString(String key, String value) {
            try {
                mConfig.put(key, value);
            } catch (JSONException e) {
                // Intentionally do nothing.
            }
            return this;
        }

        /**
         * Build a {@link CronetEngine} using this builder's configuration.
         */
        public CronetEngine build() {
            return createContext(this);
        }
    }

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
     * @param url {@link URL} for the request.
     * @param listener callback class that gets called on different events.
     * @param executor {@link Executor} on which all callbacks will be called.
     * @return new request.
     * @deprecated Use {@link Builder#build}.
     */
    @Deprecated
    public abstract UrlRequest createRequest(
            String url, UrlRequestListener listener, Executor executor);

    /**
     * Creates a {@link UrlRequest} object. All callbacks will
     * be called on {@code executor}'s thread. {@code executor} must not run
     * tasks on the current thread to prevent blocking networking operations
     * and causing exceptions during shutdown.
     *
     * @param url {@link URL} for the request.
     * @param listener callback class that gets called on different events.
     * @param executor {@link Executor} on which all callbacks will be called.
     * @param priority priority of the request which should be one of the
     *         {@link UrlRequest#REQUEST_PRIORITY_IDLE REQUEST_PRIORITY_*}
     *         values.
     * @return new request.
     * @deprecated Use {@link Builder#build}.
     */
    @Deprecated
    public abstract UrlRequest createRequest(
            String url, UrlRequestListener listener, Executor executor, int priority);

    /**
     * @return {@code true} if the engine is enabled.
     */
    abstract boolean isEnabled();

    /**
     * @return a human-readable version string of the engine.
     */
    public abstract String getVersionString();

    /**
     * Shuts down the {@link CronetEngine} if there are no active requests,
     * otherwise throws an exception.
     *
     * Cannot be called on network thread - the thread Cronet calls into
     * Executor on (which is different from the thread the Executor invokes
     * callbacks on). May block until all the {@code CronetEngine}'s
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
     * Enables the network quality estimator, which collects and reports
     * measurements of round trip time (RTT) and downstream throughput at
     * various layers of the network stack. After enabling the estimator,
     * listeners of RTT and throughput can be added with
     * {@link #addRttListener} and {@link #addThroughputListener} and
     * removed with {@link #removeRttListener} and
     * {@link #removeThroughputListener}. The estimator uses memory and CPU
     * only when enabled.
     * @param executor an executor that will be used to notified all
     *            added RTT and throughput listeners.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated public abstract void enableNetworkQualityEstimator(Executor executor);

    /**
     * Enables the network quality estimator for testing. This must be called
     * before round trip time and throughput listeners are added. Set both
     * boolean parameters to false for default behavior.
     * @param useLocalHostRequests include requests to localhost in estimates.
     * @param useSmallerResponses include small responses in throughput estimates.
     * @param executor an {@link java.util.concurrent.Executor} on which all
     *            listeners will be called.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated
    abstract void enableNetworkQualityEstimatorForTesting(
            boolean useLocalHostRequests, boolean useSmallerResponses, Executor executor);

    /**
     * Registers a listener that gets called whenever the network quality
     * estimator witnesses a sample round trip time. This must be called
     * after {@link #enableNetworkQualityEstimator}, and with throw an
     * exception otherwise. Round trip times may be recorded at various layers
     * of the network stack, including TCP, QUIC, and at the URL request layer.
     * The listener is called on the {@link java.util.concurrent.Executor} that
     * is passed to {@link #enableNetworkQualityEstimator}.
     * @param listener the listener of round trip times.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated public abstract void addRttListener(NetworkQualityRttListener listener);

    /**
     * Removes a listener of round trip times if previously registered with
     * {@link #addRttListener}. This should be called after a
     * {@link NetworkQualityRttListener} is added in order to stop receiving
     * observations.
     * @param listener the listener of round trip times.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated public abstract void removeRttListener(NetworkQualityRttListener listener);

    /**
     * Registers a listener that gets called whenever the network quality
     * estimator witnesses a sample throughput measurement. This must be called
     * after {@link #enableNetworkQualityEstimator}. Throughput observations
     * are computed by measuring bytes read over the active network interface
     * at times when at least one URL response is being received. The listener
     * is called on the {@link java.util.concurrent.Executor} that is passed to
     * {@link #enableNetworkQualityEstimator}.
     * @param listener the listener of throughput.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated
    public abstract void addThroughputListener(NetworkQualityThroughputListener listener);

    /**
     * Removes a listener of throughput. This should be called after a
     * {@link NetworkQualityThroughputListener} is added with
     * {@link #addThroughputListener} in order to stop receiving observations.
     * @param listener the listener of throughput.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated
    public abstract void removeThroughputListener(NetworkQualityThroughputListener listener);

    /**
     * Establishes a new connection to the resource specified by the {@link URL} {@code url}.
     * <p>
     * <b>Note:</b> Cronet's {@link java.net.HttpURLConnection} implementation is subject to certain
     * limitations, see {@link #createURLStreamHandlerFactory} for details.
     *
     * @param url URL of resource to connect to.
     * @return an {@link java.net.HttpURLConnection} instance implemented by this CronetEngine.
     */
    public abstract URLConnection openConnection(URL url);

    /**
     * Establishes a new connection to the resource specified by the {@link URL} {@code url}
     * using the given proxy.
     * <p>
     * <b>Note:</b> Cronet's {@link java.net.HttpURLConnection} implementation is subject to certain
     * limitations, see {@link #createURLStreamHandlerFactory} for details.
     *
     * @param url URL of resource to connect to.
     * @param proxy proxy to use when establishing connection.
     * @return an {@link java.net.HttpURLConnection} instance implemented by this CronetEngine.
     * @hide
     * @deprecated Marked as deprecated because @hide doesn't properly hide but
     *         javadocs are built with nodeprecated="yes".
     *         TODO(pauljensen): Expose once implemented, http://crbug.com/418111
     */
    public abstract URLConnection openConnection(URL url, Proxy proxy);

    /**
     * Creates a {@link URLStreamHandlerFactory} to handle HTTP and HTTPS
     * traffic. An instance of this class can be installed via
     * {@link URL#setURLStreamHandlerFactory} thus using this CronetEngine by default for
     * all requests created via {@link URL#openConnection}.
     * <p>
     * Cronet does not use certain HTTP features provided via the system:
     * <ul>
     * <li>the HTTP cache installed via
     *     {@link android.net.http.HttpResponseCache#install(java.io.File, long)
     *            HttpResponseCache.install()}</li>
     * <li>the HTTP authentication method installed via
     *     {@link java.net.Authenticator#setDefault}</li>
     * <li>the HTTP cookie storage installed via {@link java.net.CookieHandler#setDefault}</li>
     * </ul>
     * <p>
     * While Cronet supports and encourages requests using the HTTPS protocol,
     * Cronet does not provide support for the
     * {@link javax.net.ssl.HttpsURLConnection} API. This lack of support also
     * includes not using certain HTTPS features provided via the system:
     * <ul>
     * <li>the HTTPS hostname verifier installed via {@link
     *   javax.net.ssl.HttpsURLConnection#setDefaultHostnameVerifier(javax.net.ssl.HostnameVerifier)
     *     HttpsURLConnection.setDefaultHostnameVerifier()}</li>
     * <li>the HTTPS socket factory installed via {@link
     *   javax.net.ssl.HttpsURLConnection#setDefaultSSLSocketFactory(javax.net.ssl.SSLSocketFactory)
     *     HttpsURLConnection.setDefaultSSLSocketFactory()}</li>
     * </ul>
     *
     * @return an {@link URLStreamHandlerFactory} instance implemented by this
     *         CronetEngine.
     */
    public abstract URLStreamHandlerFactory createURLStreamHandlerFactory();

    /**
     * Creates a {@link CronetEngine} with the given {@link Builder}.
     * @param context Android {@link Context}.
     * @param config engine configuration.
     * @deprecated Use {@link CronetEngine.Builder}.
     */
    @Deprecated
    public static CronetEngine createContext(Builder builder) {
        CronetEngine cronetEngine = null;
        if (builder.getUserAgent().isEmpty()) {
            builder.setUserAgent(builder.getDefaultUserAgent());
        }
        if (!builder.legacyMode()) {
            cronetEngine = createCronetEngine(builder);
        }
        if (cronetEngine == null) {
            // TODO(mef): Fallback to stub implementation. Once stub
            // implementation is available merge with createCronetFactory.
            cronetEngine = createCronetEngine(builder);
        }
        Log.i(TAG, "Using network stack: " + cronetEngine.getVersionString());
        return cronetEngine;
    }

    private static CronetEngine createCronetEngine(Builder builder) {
        CronetEngine cronetEngine = null;
        try {
            Class<? extends CronetEngine> engineClass =
                    CronetEngine.class.getClassLoader()
                            .loadClass(CRONET_URL_REQUEST_CONTEXT)
                            .asSubclass(CronetEngine.class);
            Constructor<? extends CronetEngine> constructor =
                    engineClass.getConstructor(Builder.class);
            CronetEngine possibleEngine = constructor.newInstance(builder);
            if (possibleEngine.isEnabled()) {
                cronetEngine = possibleEngine;
            }
        } catch (ClassNotFoundException e) {
            // Leave as null.
        } catch (Exception e) {
            throw new IllegalStateException("Cannot instantiate: " + CRONET_URL_REQUEST_CONTEXT, e);
        }
        return cronetEngine;
    }
}
