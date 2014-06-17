// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

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

    public boolean legacyMode() {
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
