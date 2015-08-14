// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;

/**
 * A config for UrlRequestContext, which allows runtime configuration of
 * UrlRequestContext.
 */
public class UrlRequestContextConfig {
    private final JSONObject mConfig;

    /**
     * Default config enables SPDY, disables QUIC, SDCH and http cache.
     */
    public UrlRequestContextConfig() {
        mConfig = new JSONObject();
        enableLegacyMode(false);
        enableQUIC(false);
        enableHTTP2(true);
        enableSDCH(false);
        enableHttpCache(HTTP_CACHE_DISABLED, 0);
    }

    /**
     * Creates a config from a JSON string, which was serialized using
     * {@link #toString}.
     */
    public UrlRequestContextConfig(String json) throws JSONException {
        mConfig = new JSONObject(json);
    }

    /**
     * Overrides the user-agent header for all requests.
     * @return the config to facilitate chaining.
     */
    public UrlRequestContextConfig setUserAgent(String userAgent) {
        return putString(UrlRequestContextConfigList.USER_AGENT, userAgent);
    }

    String userAgent() {
        return mConfig.optString(UrlRequestContextConfigList.USER_AGENT);
    }

    /**
     * Sets directory for HTTP Cache and Cookie Storage. The directory must
     * exist.
     * @param value path to existing directory.
     * @return the config to facilitate chaining.
     */
    public UrlRequestContextConfig setStoragePath(String value) {
        if (!new File(value).isDirectory()) {
            throw new IllegalArgumentException(
                    "Storage path must be set to existing directory");
        }

        return putString(UrlRequestContextConfigList.STORAGE_PATH, value);
    }

    String storagePath() {
        return mConfig.optString(UrlRequestContextConfigList.STORAGE_PATH);
    }

    /**
     * Sets whether falling back to implementation based on system's
     * {@link java.net.HttpURLConnection} implementation is enabled.
     * Defaults to disabled.
     * @return the config to facilitate chaining.
     */
    UrlRequestContextConfig enableLegacyMode(boolean value) {
        return putBoolean(UrlRequestContextConfigList.ENABLE_LEGACY_MODE,
                          value);
    }

    boolean legacyMode() {
        return mConfig.optBoolean(
                UrlRequestContextConfigList.ENABLE_LEGACY_MODE);
    }

    /**
     * Overrides the name of the native library backing Cronet.
     * @return the config to facilitate chaining.
     */
    UrlRequestContextConfig setLibraryName(String libName) {
        return putString(UrlRequestContextConfigList.NATIVE_LIBRARY_NAME,
                         libName);
    }

    String libraryName() {
        return mConfig.optString(
                UrlRequestContextConfigList.NATIVE_LIBRARY_NAME, "cronet");
    }

    /**
     * Sets whether <a href="https://www.chromium.org/quic">QUIC</a> protocol
     * is enabled. Defaults to disabled.
     * @return the config to facilitate chaining.
     */
    public UrlRequestContextConfig enableQUIC(boolean value) {
        return putBoolean(UrlRequestContextConfigList.ENABLE_QUIC, value);
    }

    /**
     * Sets whether <a href="https://tools.ietf.org/html/rfc7540">HTTP/2</a>
     * protocol is enabled. Defaults to enabled.
     * @return the config to facilitate chaining.
     */
    public UrlRequestContextConfig enableHTTP2(boolean value) {
        return putBoolean(UrlRequestContextConfigList.ENABLE_SPDY, value);
    }

    /**
     * Sets whether
     * <a
     * href="https://lists.w3.org/Archives/Public/ietf-http-wg/2008JulSep/att-0441/Shared_Dictionary_Compression_over_HTTP.pdf">
     * SDCH</a> compression is enabled. Defaults to disabled.
     * @return the config to facilitate chaining.
     */
    public UrlRequestContextConfig enableSDCH(boolean value) {
        return putBoolean(UrlRequestContextConfigList.ENABLE_SDCH, value);
    }

    /**
     * Enables
     * <a href="https://developer.chrome.com/multidevice/data-compression">Data
     * Reduction Proxy</a>. Defaults to disabled.
     * @param key key to use when authenticating with the proxy.
     * @return the config to facilitate chaining.
     */
    public UrlRequestContextConfig enableDataReductionProxy(String key) {
        return (putString(
                UrlRequestContextConfigList.DATA_REDUCTION_PROXY_KEY, key));
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
     * @return the config to facilitate chaining.
     * @hide
     */
    public UrlRequestContextConfig setDataReductionProxyOptions(
            String primaryProxy,
            String fallbackProxy,
            String secureProxyCheckUrl) {
        if (primaryProxy.isEmpty() || fallbackProxy.isEmpty()
                || secureProxyCheckUrl.isEmpty()) {
            throw new IllegalArgumentException(
                    "Primary and fallback proxies and check url must be set");
        }
        putString(UrlRequestContextConfigList.DATA_REDUCTION_PRIMARY_PROXY,
                primaryProxy);
        putString(UrlRequestContextConfigList.DATA_REDUCTION_FALLBACK_PROXY,
                fallbackProxy);
        putString(UrlRequestContextConfigList
                .DATA_REDUCTION_SECURE_PROXY_CHECK_URL, secureProxyCheckUrl);
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
     * @param maxSize maximum size used to cache data (advisory and maybe
     * exceeded at times).
     * @return the config to facilitate chaining.
     */
    public UrlRequestContextConfig enableHttpCache(int cacheMode, long maxSize) {
        if (cacheMode == HTTP_CACHE_DISK || cacheMode == HTTP_CACHE_DISK_NO_HTTP) {
            if (storagePath().isEmpty()) {
                throw new IllegalArgumentException("Storage path must be set");
            }
        } else {
            if (!storagePath().isEmpty()) {
                throw new IllegalArgumentException(
                        "Storage path must be empty");
            }
        }
        putBoolean(UrlRequestContextConfigList.LOAD_DISABLE_CACHE,
                cacheMode == HTTP_CACHE_DISABLED || cacheMode == HTTP_CACHE_DISK_NO_HTTP);
        putLong(UrlRequestContextConfigList.HTTP_CACHE_MAX_SIZE, maxSize);

        switch (cacheMode) {
            case HTTP_CACHE_DISABLED:
                return putString(UrlRequestContextConfigList.HTTP_CACHE,
                        UrlRequestContextConfigList.HTTP_CACHE_DISABLED);
            case HTTP_CACHE_DISK_NO_HTTP:
            case HTTP_CACHE_DISK:
                return putString(UrlRequestContextConfigList.HTTP_CACHE,
                        UrlRequestContextConfigList.HTTP_CACHE_DISK);

            case HTTP_CACHE_IN_MEMORY:
                return putString(UrlRequestContextConfigList.HTTP_CACHE,
                        UrlRequestContextConfigList.HTTP_CACHE_MEMORY);
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
     * @return the config to facilitate chaining.
     */
    public UrlRequestContextConfig addQuicHint(String host,
            int port,
            int alternatePort) {
        if (host.contains("/")) {
            throw new IllegalArgumentException("Illegal QUIC Hint Host: "
                                               + host);
        }
        try {
            JSONArray quicHints = mConfig.optJSONArray(
                    UrlRequestContextConfigList.QUIC_HINTS);
            if (quicHints == null) {
                quicHints = new JSONArray();
                mConfig.put(UrlRequestContextConfigList.QUIC_HINTS, quicHints);
            }

            JSONObject hint = new JSONObject();
            hint.put(UrlRequestContextConfigList.QUIC_HINT_HOST, host);
            hint.put(UrlRequestContextConfigList.QUIC_HINT_PORT, port);
            hint.put(UrlRequestContextConfigList.QUIC_HINT_ALT_PORT,
                     alternatePort);
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
     * @return the config to facilitate chaining.
     */
    public UrlRequestContextConfig setExperimentalQuicConnectionOptions(
            String quicConnectionOptions) {
        return putString(UrlRequestContextConfigList.QUIC_OPTIONS,
                         quicConnectionOptions);
    }

    /**
     * Get JSON string representation of the config.
     */
    @Override
    public String toString() {
        return mConfig.toString();
    }

    /**
     * Sets a boolean value in the config. Returns a reference to the same
     * config object, so you can chain put calls together.
     * @return the config to facilitate chaining.
     */
    private UrlRequestContextConfig putBoolean(String key, boolean value) {
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
     * @return the config to facilitate chaining.
     */
    private UrlRequestContextConfig putLong(String key, long value)  {
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
     * @return the config to facilitate chaining.
     */
    private UrlRequestContextConfig putString(String key, String value) {
        try {
            mConfig.put(key, value);
        } catch (JSONException e) {
            // Intentionally do nothing.
        }
        return this;
    }
}
