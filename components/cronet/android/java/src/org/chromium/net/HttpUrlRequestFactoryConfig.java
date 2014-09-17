// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * A config for HttpUrlRequestFactory, which allows runtime configuration of
 * HttpUrlRequestFactory.
 */
public class HttpUrlRequestFactoryConfig {

    /**
     * Default config enables SPDY, QUIC, in memory http cache.
     */
    public HttpUrlRequestFactoryConfig() {
        enableLegacyMode(false);
        enableQUIC(false);
        enableSPDY(true);
        enableHttpCache(HttpCache.IN_MEMORY, 100 * 1024);
    }

    /**
     * Override the name of the native library backing cronet.
     */
    public HttpUrlRequestFactoryConfig setLibraryName(String libName) {
        return putString(UrlRequestContextConfig.NATIVE_LIBRARY_NAME, libName);
    }

    /**
     * Create config from json serialized using @toString.
     */
    public HttpUrlRequestFactoryConfig(String json) throws JSONException {
        mConfig = new JSONObject(json);
    }

    /**
     * Boolean, use HttpUrlRequest-based implementation if true. All other
     * keys are not applicable.
     */
    public HttpUrlRequestFactoryConfig enableLegacyMode(boolean value) {
        return putBoolean(UrlRequestContextConfig.ENABLE_LEGACY_MODE, value);
    }

    boolean legacyMode() {
        return mConfig.optBoolean(UrlRequestContextConfig.ENABLE_LEGACY_MODE);
    }

    /**
     * Boolean, enable QUIC if true.
     */
    public HttpUrlRequestFactoryConfig enableQUIC(boolean value) {
        return putBoolean(UrlRequestContextConfig.ENABLE_QUIC, value);
    }

    /**
     * Boolean, enable SPDY if true.
     */
    public HttpUrlRequestFactoryConfig enableSPDY(boolean value) {
        return putBoolean(UrlRequestContextConfig.ENABLE_SPDY, value);
    }

    String libraryName() {
        return mConfig.optString(UrlRequestContextConfig.NATIVE_LIBRARY_NAME,
                                 "cronet");
    }

    /**
     * Enumeration, Disable or Enable Disk or Memory Cache and specify its
     * maximum size in bytes.
     */
    public enum HttpCache { DISABLED, IN_MEMORY, DISK };
    public HttpUrlRequestFactoryConfig enableHttpCache(HttpCache value,
                                                       long maxSize) {
        switch(value) {
            case DISABLED:
                return putString(UrlRequestContextConfig.HTTP_CACHE,
                                 UrlRequestContextConfig.HTTP_CACHE_DISABLED);
            case DISK:
                putLong(UrlRequestContextConfig.HTTP_CACHE_MAX_SIZE, maxSize);
                return putString(UrlRequestContextConfig.HTTP_CACHE,
                                 UrlRequestContextConfig.HTTP_CACHE_DISK);
            case IN_MEMORY:
                putLong(UrlRequestContextConfig.HTTP_CACHE_MAX_SIZE, maxSize);
                return putString(UrlRequestContextConfig.HTTP_CACHE,
                                 UrlRequestContextConfig.HTTP_CACHE_MEMORY);
        }
        return this;
    }

    /**
     * String, path to directory for HTTP Cache and Cookie Storage.
     */
    public HttpUrlRequestFactoryConfig setStoragePath(String value) {
        return putString(UrlRequestContextConfig.STORAGE_PATH, value);
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
    public HttpUrlRequestFactoryConfig addQuicHint(String host,
                                                   int port,
                                                   int alternatePort) {
        try {
            JSONArray quicHints = mConfig.optJSONArray(
                    UrlRequestContextConfig.QUIC_HINTS);
            if (quicHints == null) {
                quicHints = new JSONArray();
                mConfig.put(UrlRequestContextConfig.QUIC_HINTS, quicHints);
            }

            JSONObject hint = new JSONObject();
            hint.put(UrlRequestContextConfig.QUIC_HINT_HOST, host);
            hint.put(UrlRequestContextConfig.QUIC_HINT_PORT, port);
            hint.put(UrlRequestContextConfig.QUIC_HINT_ALT_PORT, alternatePort);
            quicHints.put(hint);
        } catch (JSONException e) {
            ;
        }
        return this;
    }

    /**
     * Get JSON string representation of the config.
     */
    public String toString() {
        return mConfig.toString();
    }

    /**
     * Sets a boolean value in the config. Returns a reference to the same
     * config object, so you can chain put calls together.
     */
    private HttpUrlRequestFactoryConfig putBoolean(String key, boolean value) {
        try {
            mConfig.put(key, value);
        } catch (JSONException e) {
            ;
        }
        return this;
    }

    /**
     * Sets a long value in the config. Returns a reference to the same
     * config object, so you can chain put calls together.
     */
    private HttpUrlRequestFactoryConfig putLong(String key, long value)  {
        try {
            mConfig.put(key, value);
        } catch (JSONException e) {
            ;
        }
        return this;
    }

    /**
     * Sets a string value in the config. Returns a reference to the same
     * config object, so you can chain put calls together.
     */
    private HttpUrlRequestFactoryConfig putString(String key, String value) {
        try {
            mConfig.put(key, value);
        } catch (JSONException e) {
            ;
        }
        return this;
    }

    private JSONObject mConfig = new JSONObject();
}
