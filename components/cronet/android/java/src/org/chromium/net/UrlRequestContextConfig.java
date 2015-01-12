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

    /**
     * Default config enables SPDY, QUIC, in memory http cache.
     */
    public UrlRequestContextConfig() {
        enableLegacyMode(false);
        enableQUIC(false);
        enableSPDY(true);
        enableHttpCache(HttpCache.DISABLED, 0);
    }

    /**
     * Create config from json serialized using @toString.
     */
    public UrlRequestContextConfig(String json) throws JSONException {
        mConfig = new JSONObject(json);
    }

    /**
     * Override the user-agent header for all requests.
     */
    public UrlRequestContextConfig setUserAgent(String userAgent) {
        return putString(UrlRequestContextConfigList.USER_AGENT, userAgent);
    }

    String userAgent() {
        return mConfig.optString(UrlRequestContextConfigList.USER_AGENT);
    }

    /**
     * String, path to existing directory for HTTP Cache and Cookie Storage.
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
     * Boolean, use HttpUrlConnection-based implementation if true. All other
     * keys are not applicable.
     */
    public UrlRequestContextConfig enableLegacyMode(boolean value) {
        return putBoolean(UrlRequestContextConfigList.ENABLE_LEGACY_MODE,
                          value);
    }

    boolean legacyMode() {
        return mConfig.optBoolean(
                UrlRequestContextConfigList.ENABLE_LEGACY_MODE);
    }

    /**
     * Override the name of the native library backing cronet.
     */
    public UrlRequestContextConfig setLibraryName(String libName) {
        return putString(UrlRequestContextConfigList.NATIVE_LIBRARY_NAME,
                         libName);
    }

    String libraryName() {
        return mConfig.optString(
                UrlRequestContextConfigList.NATIVE_LIBRARY_NAME, "cronet");
    }

    /**
     * Boolean, enable QUIC if true.
     */
    public UrlRequestContextConfig enableQUIC(boolean value) {
        return putBoolean(UrlRequestContextConfigList.ENABLE_QUIC, value);
    }

    /**
     * Boolean, enable SPDY if true.
     */
    public UrlRequestContextConfig enableSPDY(boolean value) {
        return putBoolean(UrlRequestContextConfigList.ENABLE_SPDY, value);
    }

    /**
     * Enumeration, disable or enable cache in memory or on disk.
     */
    public enum HttpCache {
        /**
         * Disable cache, some data may still be temporarily stored in memory.
         */
        DISABLED,

        /**
         * Enable in memory cache, including HTTP data.
         */
        IN_MEMORY,

        /**
         * Enable disk cache, excluding HTTP data.
         */
        DISK_NO_HTTP,

        /**
         * Enable on disk cache, including HTTP data.
         */
        DISK
    };

    /**
     * Enable or disable caching of http data and other information like QUIC
     * server information.
     * @param cacheMode control location and type of cached data.
     * @param maxSize maximum size used to cache data (advisory and maybe
     * exceeded at times).
     */
    public UrlRequestContextConfig enableHttpCache(HttpCache cacheMode,
            long maxSize) {
        if (cacheMode == HttpCache.DISK
                || cacheMode == HttpCache.DISK_NO_HTTP) {
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
                cacheMode == HttpCache.DISABLED
                || cacheMode == HttpCache.DISK_NO_HTTP);
        putLong(UrlRequestContextConfigList.HTTP_CACHE_MAX_SIZE, maxSize);

        switch (cacheMode) {
            case DISABLED:
                return putString(UrlRequestContextConfigList.HTTP_CACHE,
                        UrlRequestContextConfigList.HTTP_CACHE_DISABLED);
            case DISK_NO_HTTP:
            case DISK:
                return putString(UrlRequestContextConfigList.HTTP_CACHE,
                        UrlRequestContextConfigList.HTTP_CACHE_DISK);

            case IN_MEMORY:
                return putString(UrlRequestContextConfigList.HTTP_CACHE,
                        UrlRequestContextConfigList.HTTP_CACHE_MEMORY);
        }
        return this;
    }

    /**
     * Explicitly mark |host| as supporting QUIC.
     * Note that enableHttpCache(DISK) is needed to take advantage of 0-RTT
     * connection establishment between sessions.
     *
     * @param host of the server that supports QUIC.
     * @param port of the server that supports QUIC.
     * @param alternatePort to use for QUIC.
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
     */
    private UrlRequestContextConfig putString(String key, String value) {
        try {
            mConfig.put(key, value);
        } catch (JSONException e) {
            // Intentionally do nothing.
        }
        return this;
    }

    private JSONObject mConfig = new JSONObject();
}
