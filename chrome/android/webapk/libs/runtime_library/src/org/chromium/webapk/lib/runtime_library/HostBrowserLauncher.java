// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.runtime_library;

import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkConstants;

import java.io.ByteArrayOutputStream;

/**
 * Launches Chrome in WebAPK mode.
 */
public class HostBrowserLauncher {
    private static final String META_DATA_RUNTIME_HOST = "runtimeHost";
    private static final String META_DATA_START_URL = "startUrl";
    private static final String META_DATA_NAME = "name";
    private static final String META_DATA_DISPLAY_MODE = "displayMode";
    private static final String META_DATA_ORIENTATION = "orientation";
    private static final String META_DATA_THEME_COLOR = "themeColor";
    private static final String META_DATA_BACKGROUND_COLOR = "backgroundColor";
    private static final String META_DATA_ICON_URL = "iconUrl";

    /**
     * Key for passing app icon id in Bundle to {@link #launch()}.
     */
    private static final String KEY_APP_ICON_ID = "app_icon_id";

    private static final String TAG = "cr_HostBrowserLauncher";

    /**
     * Launches Chrome in WebAPK mode.
     * @param context Application context.
     * @param intent Intent used to launch WebAPK.
     * @param bundle Contains extra parameters.
     */
    public void launch(Context context, Intent intent, Bundle bundle) {
        int appIconId = bundle.getInt(KEY_APP_ICON_ID);

        String packageName = context.getPackageName();
        ApplicationInfo appInfo;
        try {
            appInfo = context.getPackageManager().getApplicationInfo(
                    packageName, PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            return;
        }

        Bundle metaBundle = appInfo.metaData;
        String url = metaBundle.getString(META_DATA_START_URL);

        String overrideUrl = intent.getDataString();
        // TODO(pkotwicz): Use same logic as {@code IntentHandler#shouldIgnoreIntent()}
        if (overrideUrl != null && overrideUrl.startsWith("https:")) {
            url = overrideUrl;
        }
        int source = intent.getIntExtra(WebApkConstants.EXTRA_SOURCE, 0);

        String webappId = WebApkConstants.WEBAPK_ID_PREFIX + packageName;
        String runtimeHost = metaBundle.getString(META_DATA_RUNTIME_HOST);
        String shortName = (String) context.getPackageManager().getApplicationLabel(appInfo);
        // TODO(hanxi): find a neat solution to avoid encode/decode each time launch the
        // activity.
        Bitmap icon = BitmapFactory.decodeResource(context.getResources(), appIconId);
        String encodedIcon = encodeBitmapAsString(icon);
        String name = metaBundle.getString(META_DATA_NAME);
        String displayMode = metaBundle.getString(META_DATA_DISPLAY_MODE);
        String orientation = metaBundle.getString(META_DATA_ORIENTATION);
        long themeColor = getLongFromBundle(metaBundle, META_DATA_THEME_COLOR);
        long backgroundColor = getLongFromBundle(metaBundle, META_DATA_BACKGROUND_COLOR);
        boolean isIconGenerated = TextUtils.isEmpty(metaBundle.getString(META_DATA_ICON_URL));
        Log.v(TAG, "Url of the WebAPK: " + url);
        Log.v(TAG, "WebappId of the WebAPK: " + webappId);
        Log.v(TAG, "Name of the WebAPK:" + name);
        Log.v(TAG, "Package name of the WebAPK:" + packageName);

        Intent newIntent = new Intent();
        newIntent.setComponent(new ComponentName(
                runtimeHost, "org.chromium.chrome.browser.webapps.WebappLauncherActivity"));

        // Chrome expects the ShortcutHelper.EXTRA_DISPLAY_MODE and
        // ShortcutHelper.EXTRA_ORIENTATION extras to be enum values. We send string extras for
        // the display mode and orientation so have to use different keys.
        newIntent.putExtra(WebApkConstants.EXTRA_ID, webappId)
                .putExtra(WebApkConstants.EXTRA_SHORT_NAME, shortName)
                .putExtra(WebApkConstants.EXTRA_NAME, name)
                .putExtra(WebApkConstants.EXTRA_URL, url)
                .putExtra(WebApkConstants.EXTRA_ICON, encodedIcon)
                .putExtra(WebApkConstants.EXTRA_SOURCE, source)
                .putExtra(WebApkConstants.EXTRA_THEME_COLOR, themeColor)
                .putExtra(WebApkConstants.EXTRA_BACKGROUND_COLOR, backgroundColor)
                .putExtra(WebApkConstants.EXTRA_IS_ICON_GENERATED, isIconGenerated)
                .putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, packageName)
                .putExtra(WebApkConstants.EXTRA_WEBAPK_DISPLAY_MODE, displayMode)
                .putExtra(WebApkConstants.EXTRA_WEBAPK_ORIENTATION, orientation);

        try {
            context.startActivity(newIntent);
        } catch (ActivityNotFoundException e) {
            e.printStackTrace();
        }
    }

    /**
     * Compresses a bitmap into a PNG and converts into a Base64 encoded string.
     * The encoded string can be decoded using {@link decodeBitmapFromString(String)}.
     * @param bitmap The Bitmap to compress and encode.
     * @return the String encoding the Bitmap.
     */
    private static String encodeBitmapAsString(Bitmap bitmap) {
        if (bitmap == null) return "";
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, output);
        return Base64.encodeToString(output.toByteArray(), Base64.DEFAULT);
    }

    /**
     * Gets a long from a Bundle. The long should be terminated with 'L'. This function is more
     * reliable than Bundle#getLong() which returns 0 if the value is below Float.MAX_VALUE.
     */
    private static long getLongFromBundle(Bundle bundle, String key) {
        String value = bundle.getString(key);
        if (value == null || !value.endsWith("L")) {
            return 0;
        }
        try {
            return Long.parseLong(value.substring(0, value.length() - 1));
        } catch (NumberFormatException e) {
        }
        return 0;
    }
}
