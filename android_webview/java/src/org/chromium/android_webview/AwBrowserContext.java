// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.SharedPreferences;

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
    private static final String HTTP_AUTH_DATABASE_FILE = "http_auth.db";

    private final SharedPreferences mSharedPreferences;

    private AwGeolocationPermissions mGeolocationPermissions;
    private AwFormDatabase mFormDatabase;
    private HttpAuthDatabase mHttpAuthDatabase;
    private AwMessagePortService mMessagePortService;
    private AwMetricsServiceClient mMetricsServiceClient;
    private AwServiceWorkerController mServiceWorkerController;
    private Context mApplicationContext;

    public AwBrowserContext(SharedPreferences sharedPreferences, Context applicationContext) {
        mSharedPreferences = sharedPreferences;
        mMetricsServiceClient = new AwMetricsServiceClient(applicationContext);
        mApplicationContext = applicationContext;
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

    public HttpAuthDatabase getHttpAuthDatabase(Context context) {
        if (mHttpAuthDatabase == null) {
            mHttpAuthDatabase = HttpAuthDatabase.newInstance(context, HTTP_AUTH_DATABASE_FILE);
        }
        return mHttpAuthDatabase;
    }

    public AwMessagePortService getMessagePortService() {
        if (mMessagePortService == null) {
            mMessagePortService = new AwMessagePortService();
        }
        return mMessagePortService;
    }

    public AwMetricsServiceClient getMetricsServiceClient() {
        return mMetricsServiceClient;
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
}
