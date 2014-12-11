// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.GeolocationPermissions;
import android.webkit.ValueCallback;

import org.chromium.android_webview.AwGeolocationPermissions;

import java.util.Set;

/**
 * Chromium implementation of GeolocationPermissions -- forwards calls to the
 * chromium internal implementation.
 */
final class GeolocationPermissionsAdapter extends GeolocationPermissions {
    private AwGeolocationPermissions mChromeGeolocationPermissions;

    public GeolocationPermissionsAdapter(AwGeolocationPermissions chromeGeolocationPermissions) {
        mChromeGeolocationPermissions = chromeGeolocationPermissions;
    }

    @Override
    public void allow(String origin) {
        mChromeGeolocationPermissions.allow(origin);
    }

    @Override
    public void clear(String origin) {
        mChromeGeolocationPermissions.clear(origin);
    }

    @Override
    public void clearAll() {
        mChromeGeolocationPermissions.clearAll();
    }

    @Override
    public void getAllowed(String origin, ValueCallback<Boolean> callback) {
        mChromeGeolocationPermissions.getAllowed(origin, callback);
    }

    @Override
    public void getOrigins(ValueCallback<Set<String>> callback) {
        mChromeGeolocationPermissions.getOrigins(callback);
    }
}
