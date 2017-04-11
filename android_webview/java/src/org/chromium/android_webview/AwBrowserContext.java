// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler;
import org.chromium.content.browser.ContentViewStatics;

/**
 * Java side of the Browser Context: contains all the java side objects needed to host one
 * browing session (i.e. profile).
 *
 * Note that historically WebView was running in single process mode, and limitations on renderer
 * process only being able to use a single browser context, currently there can only be one
 * AwBrowserContext instance, so at this point the class mostly exists for conceptual clarity.
 */
public class AwBrowserContext {
    private static final String TAG = "AwBrowserContext";
    private final SharedPreferences mSharedPreferences;

    private AwGeolocationPermissions mGeolocationPermissions;
    private AwFormDatabase mFormDatabase;
    private AwServiceWorkerController mServiceWorkerController;
    private Context mApplicationContext;

    public AwBrowserContext(SharedPreferences sharedPreferences, Context applicationContext) {
        mSharedPreferences = sharedPreferences;
        mApplicationContext = applicationContext;

        initSafeBrowsingApiHandler();
    }

    public AwGeolocationPermissions getGeolocationPermissions() {
        if (mGeolocationPermissions == null) {
            mGeolocationPermissions = new AwGeolocationPermissions(mSharedPreferences);
        }
        return mGeolocationPermissions;
    }

    public AwFormDatabase getFormDatabase() {
        if (mFormDatabase == null) {
            mFormDatabase = new AwFormDatabase();
        }
        return mFormDatabase;
    }

    public AwServiceWorkerController getServiceWorkerController() {
        if (mServiceWorkerController == null) {
            mServiceWorkerController = new AwServiceWorkerController(mApplicationContext, this);
        }
        return mServiceWorkerController;
    }

    /**
     * @see android.webkit.WebView#pauseTimers()
     */
    public void pauseTimers() {
        ContentViewStatics.setWebKitSharedTimersSuspended(true);
    }

    /**
     * @see android.webkit.WebView#resumeTimers()
     */
    public void resumeTimers() {
        ContentViewStatics.setWebKitSharedTimersSuspended(false);
    }

    @SuppressWarnings("unchecked")
    private void initSafeBrowsingApiHandler() {
        final String safeBrowsingApiHandler =
                "com.android.webview.chromium.AwSafeBrowsingApiHandler";

        // Try to get a specialized service bridge.
        try {
            Class<? extends SafeBrowsingApiHandler> cls =
                    (Class<? extends SafeBrowsingApiHandler>) Class.forName(safeBrowsingApiHandler);
            SafeBrowsingApiBridge.setSafeBrowsingHandlerType(cls);
        } catch (ClassNotFoundException e) {
            // This is not an error; it just means this device doesn't have specialized services.
        }
    }
}
