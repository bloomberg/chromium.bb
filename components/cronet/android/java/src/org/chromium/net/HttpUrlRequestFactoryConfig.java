// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.json.JSONException;

/**
 * A config for HttpUrlRequestFactory, which allows runtime configuration of
 * HttpUrlRequestFactory.
 */
public class HttpUrlRequestFactoryConfig extends UrlRequestContextConfig {

    /**
     * Default config enables SPDY, QUIC, in memory http cache.
     */
    public HttpUrlRequestFactoryConfig() {
        super();
    }

    /**
     * Create config from json serialized using @toString.
     */
    public HttpUrlRequestFactoryConfig(String json) throws JSONException {
        super(json);
    }
}
