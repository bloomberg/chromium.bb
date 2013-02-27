// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.SharedPreferences;

/**
 * Java side of the Browser Context: contains all the java side objects needed to host one
 * browing session (i.e. profile).
 * Note that due to running in single process mode, and limitations on renderer process only
 * being able to use a single browser context, currently there can only be one AwBrowserContext
 * instance, so at this point the class mostly exists for conceptual clarity.
 *
 * Obtain the default (singleton) instance with  AwBrowserProcess.getDefaultBrowserContext().
 */
public class AwBrowserContext {

    private SharedPreferences mSharedPreferences;

    private AwGeolocationPermissions mGeolocationPermissions;
    private AwCookieManager mCookieManager;

    public AwBrowserContext(SharedPreferences sharedPreferences) {
        mSharedPreferences = sharedPreferences;
    }

    public AwGeolocationPermissions getGeolocationPermissions() {
        if (mGeolocationPermissions == null) {
            mGeolocationPermissions = new AwGeolocationPermissions(mSharedPreferences);
        }
        return mGeolocationPermissions;
    }

    public AwCookieManager getCookieManager() {
        if (mCookieManager == null) {
            mCookieManager = new AwCookieManager();
        }
        return mCookieManager;
    }
}