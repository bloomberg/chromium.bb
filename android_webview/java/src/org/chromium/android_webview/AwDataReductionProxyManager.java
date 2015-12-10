// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.SQLException;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Handler;
import android.util.Log;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;

import java.lang.reflect.Field;

/**
 * Controls data reduction proxy.
 */
public final class AwDataReductionProxyManager {
    // The setting Uri. Used when querying GoogleSettings.
    // TODO: This GoogleSettings Uri does not exist yet!
    private static final Uri CONTENT_URI = Uri.parse("content://com.google.settings/partner");

    // Setting name for allowing data reduction proxy. Used when querying GoogleSettings.
    // Setting type: int ( 0 = disallow, 1 = allow )
    private static final String WEBVIEW_DATA_REDUCTION_PROXY = "use_webview_data_reduction_proxy";

    private static final String DRP_CLASS = "com.android.webview.chromium.Drp";
    private static final String TAG = "DataReductionProxySettingListener";

    // This is the same as Chromium data_reduction_proxy::switches::kEnableDataReductionProxy.
    private static final String ENABLE_DATA_REDUCTION_PROXY = "enable-spdy-proxy-auth";
    // This is the same as Chromium data_reduction_proxy::switches::kDataReductionProxyKey.
    private static final String DATA_REDUCTION_PROXY_KEY = "spdy-proxy-auth-value";

    private ContentObserver mProxySettingObserver;

    public AwDataReductionProxyManager() {}

    public void start(final Context context) {
        // This is the DRP key embedded in WebView apk.
        final String embeddedKey = readKey();

        // Developers could test DRP by passing ENABLE_DATA_REDUCTION_PROXY and (optionally)
        // DATA_REDUCTION_PROXY_KEY to the commandline switches.  In this case, we will try to
        // initialize DRP from commandline. And ignore user's preference.  If
        // DATA_REDUCTION_PROXY_KEY is specified in commandline, use it.  Otherwise, use the key
        // embedded in WebView apk.
        CommandLine cl = CommandLine.getInstance();
        if (cl.hasSwitch(ENABLE_DATA_REDUCTION_PROXY)) {
            String key = cl.getSwitchValue(DATA_REDUCTION_PROXY_KEY, embeddedKey);
            if (key == null || key.isEmpty()) {
                return;
            }

            // Now we will enable DRP because we've got a commandline switch to enable it.
            // We won't listen to Google Settings preference change because commandline switches
            // trump that.
            AwContentsStatics.setDataReductionProxyKey(key);
            AwContentsStatics.setDataReductionProxyEnabled(true);
            return;
        }

        // Now, there is no commandline switches to enable DRP, and reading the
        // DRP key from WebView apk failed. Just return and leave DRP disabled.
        if (embeddedKey == null || embeddedKey.isEmpty()) {
            return;
        }

        applyDataReductionProxySettingsAsync(context, embeddedKey);
        ContentResolver resolver = context.getContentResolver();
        mProxySettingObserver = new ContentObserver(new Handler()) {
            @Override
            public void onChange(boolean selfChange, Uri uri) {
                applyDataReductionProxySettingsAsync(context, embeddedKey);
            }
        };
        resolver.registerContentObserver(
                Uri.withAppendedPath(CONTENT_URI, WEBVIEW_DATA_REDUCTION_PROXY), false,
                mProxySettingObserver);
    }

    private String readKey() {
        try {
            Class<?> cls = Class.forName(DRP_CLASS);
            Field f = cls.getField("KEY");
            return (String) f.get(null);
        } catch (ClassNotFoundException ex) {
            Log.e(TAG, "No DRP key due to exception:" + ex);
        } catch (NoSuchFieldException ex) {
            Log.e(TAG, "No DRP key due to exception:" + ex);
        } catch (SecurityException ex) {
            Log.e(TAG, "No DRP key due to exception:" + ex);
        } catch (IllegalArgumentException ex) {
            Log.e(TAG, "No DRP key due to exception:" + ex);
        } catch (IllegalAccessException ex) {
            Log.e(TAG, "No DRP key due to exception:" + ex);
        } catch (NullPointerException ex) {
            Log.e(TAG, "No DRP key due to exception:" + ex);
        }
        return null;
    }

    private static void applyDataReductionProxySettingsAsync(
            final Context context, final String key) {
        AsyncTask.THREAD_POOL_EXECUTOR.execute(new Runnable() {
            @Override
            public void run() {
                final boolean enabled = isDataReductionProxyEnabled(context);

                ThreadUtils.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (enabled) {
                            // Set the data reduction proxy key.
                            AwContentsStatics.setDataReductionProxyKey(key);
                        }
                        AwContentsStatics.setDataReductionProxyEnabled(enabled);
                    }
                });
            }
        });
    }

    private static boolean isDataReductionProxyEnabled(Context context) {
        return getProxySetting(context.getContentResolver(), WEBVIEW_DATA_REDUCTION_PROXY) != 0;
    }

    // Read query setting from GoogleSettings.
    private static int getProxySetting(ContentResolver resolver, String name) {
        String value = null;
        Cursor c = null;
        try {
            c = resolver.query(
                    CONTENT_URI, new String[] {"value"}, "name=?", new String[] {name}, null);
            if (c != null && c.moveToNext()) value = c.getString(0);
        } catch (SQLException e) {
            // SQL error: return null, but don't cache it.
            Log.e(TAG, "Can't get key " + name + " from " + CONTENT_URI, e);
        } finally {
            if (c != null) c.close();
        }
        int enabled = 0;
        try {
            if (value != null) {
                enabled = Integer.parseInt(value);
            }
        } catch (NumberFormatException e) {
            Log.e(TAG, "cannot parse" + value, e);
        }
        return enabled;
    }
}
