// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.ValueCallback;
import android.webkit.WebStorage;

import org.chromium.android_webview.AwQuotaManagerBridge;

import java.util.HashMap;
import java.util.Map;

/**
 * Chromium implementation of WebStorage -- forwards calls to the
 * chromium internal implementation.
 */
@SuppressWarnings("deprecation")
final class WebStorageAdapter extends WebStorage {
    private final AwQuotaManagerBridge mQuotaManagerBridge;
    WebStorageAdapter(AwQuotaManagerBridge quotaManagerBridge) {
        mQuotaManagerBridge = quotaManagerBridge;
    }

    @Override
    public void getOrigins(final ValueCallback<Map> callback) {
        mQuotaManagerBridge.getOrigins(new ValueCallback<AwQuotaManagerBridge.Origins>() {
            @Override
            public void onReceiveValue(AwQuotaManagerBridge.Origins origins) {
                Map<String, Origin> originsMap = new HashMap<String, Origin>();
                for (int i = 0; i < origins.mOrigins.length; ++i) {
                    Origin origin = new Origin(origins.mOrigins[i], origins.mQuotas[i],
                            origins.mUsages[i]) {
                        // Intentionally empty to work around cross-package protected visibility
                        // of Origin constructor.
                    };
                    originsMap.put(origins.mOrigins[i], origin);
                }
                callback.onReceiveValue(originsMap);
            }
        });
    }

    @Override
    public void getUsageForOrigin(String origin, ValueCallback<Long> callback) {
        mQuotaManagerBridge.getUsageForOrigin(origin, callback);
    }

    @Override
    public void getQuotaForOrigin(String origin, ValueCallback<Long> callback) {
        mQuotaManagerBridge.getQuotaForOrigin(origin, callback);
    }

    @Override
    public void setQuotaForOrigin(String origin, long quota) {
        // Intentional no-op for deprecated method.
    }

    @Override
    public void deleteOrigin(String origin) {
        mQuotaManagerBridge.deleteOrigin(origin);
    }

    @Override
    public void deleteAllData() {
        mQuotaManagerBridge.deleteAllData();
    }
}
