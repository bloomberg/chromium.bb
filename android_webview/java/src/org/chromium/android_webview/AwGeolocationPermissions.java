// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.SharedPreferences;
import android.webkit.ValueCallback;

import org.chromium.base.ThreadUtils;
import org.chromium.net.GURLUtils;

import java.util.HashSet;
import java.util.Set;

/**
 * This class is used to manage permissions for the WebView's Geolocation JavaScript API.
 *
 * Callbacks are posted on the UI thread.
 */
public final class AwGeolocationPermissions {

    private static final String PREF_PREFIX =
            AwGeolocationPermissions.class.getCanonicalName() + "%";
    private final SharedPreferences mSharedPreferences;

    public AwGeolocationPermissions(SharedPreferences sharedPreferences) {
        mSharedPreferences = sharedPreferences;
    }

    public void allow(String origin) {
        String key = getOriginKey(origin);
        if (key != null) {
            mSharedPreferences.edit().putBoolean(key, true).apply();
        }
    }

    public void deny(String origin) {
        String key = getOriginKey(origin);
        if (key != null) {
            mSharedPreferences.edit().putBoolean(key, false).apply();
        }
    }

    public void clear(String origin) {
        String key = getOriginKey(origin);
        if (key != null) {
            mSharedPreferences.edit().remove(key).apply();
        }
    }

    public void clearAll() {
        SharedPreferences.Editor editor = null;
        for (String name : mSharedPreferences.getAll().keySet()) {
            if (name.startsWith(PREF_PREFIX)) {
                if (editor == null) {
                    editor = mSharedPreferences.edit();
                }
                editor.remove(name);
            }
        }
        if (editor != null) {
            editor.apply();
        }
    }

    public void getAllowed(String origin, final ValueCallback<Boolean> callback) {
        boolean allowed = false;
        try {
            String key = getOriginKey(origin);
            if (key != null) {
                allowed = mSharedPreferences.getBoolean(key, false);
            }
        } catch (ClassCastException e) {
            // Want to return false in this case, do nothing here
        }
        final boolean finalAllowed = allowed;
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                callback.onReceiveValue(finalAllowed);
            }
        });
    }

    public void getOrigins(final ValueCallback<Set<String>> callback) {
        final Set<String> origins = new HashSet<String>();
        for (String name : mSharedPreferences.getAll().keySet()) {
            if (name.startsWith(PREF_PREFIX)) {
                origins.add(name.substring(PREF_PREFIX.length()));
            }
        }
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                callback.onReceiveValue(origins);
            }
        });
    }

    private String getOriginKey(String url) {
        String origin = GURLUtils.getOrigin(url);
        if (origin.isEmpty()) {
            return null;
        }

        return PREF_PREFIX + origin;
    }
}
