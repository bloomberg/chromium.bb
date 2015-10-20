// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.json.JSONException;

/**
 * A config for CronetEngine, which allows runtime configuration of
 * CronetEngine.
 * @deprecated use {@link CronetEngine.Builder} instead.
 */
public class UrlRequestContextConfig extends CronetEngine.Builder {
    public UrlRequestContextConfig() {
        // Context will be passed in later when the ChromiumUrlRequestFactory
        // or ChromiumUrlRequestContext is created.
        super(null);
    }

    public UrlRequestContextConfig(String json) throws JSONException {
        // Context will be passed in later when the ChromiumUrlRequestFactory
        // or ChromiumUrlRequestContext is created.
        super(null, json);
    }
}
