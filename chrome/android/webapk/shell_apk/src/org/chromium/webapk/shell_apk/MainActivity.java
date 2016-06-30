// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.content.ComponentName;
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
 * WebAPK's main Activity.
 */
public class MainActivity extends Activity {
    // These EXTRA_* values must stay in sync with
    // {@link org.chromium.chrome.browser.ShortcutHelper}.
    private static final String EXTRA_ID = "org.chromium.chrome.browser.webapp_id";
    private static final String EXTRA_ICON = "org.chromium.chrome.browser.webapp_icon";
    private static final String EXTRA_SHORT_NAME = "org.chromium.chrome.browser.webapp_short_name";
    private static final String EXTRA_NAME = "org.chromium.chrome.browser.webapp_name";
    private static final String EXTRA_URL = "org.chromium.chrome.browser.webapp_url";
    private static final String EXTRA_SOURCE = "org.chromium.chrome.browser.webapp_source";
    private static final String EXTRA_THEME_COLOR = "org.chromium.chrome.browser.theme_color";
    private static final String EXTRA_BACKGROUND_COLOR =
            "org.chromium.chrome.browser.background_color";
    private static final String EXTRA_IS_ICON_GENERATED =
            "org.chromium.chrome.browser.is_icon_generated";
    private static final String EXTRA_WEBAPK_PACKAGE_NAME =
            "org.chromium.chrome.browser.webapk_package_name";

    private static final String META_DATA_RUNTIME_HOST = "runtimeHost";
    private static final String META_DATA_START_URL = "startUrl";
    private static final String META_DATA_NAME = "name";
    private static final String META_DATA_DISPLAY_MODE = "displayMode";
    private static final String META_DATA_ORIENTATION = "orientation";
    private static final String META_DATA_THEME_COLOR = "themeColor";
    private static final String META_DATA_BACKGROUND_COLOR = "backgroundColor";
    private static final String META_DATA_ICON_URL = "iconUrl";

    private static final String TAG = "cr_MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String packageName = getPackageName();
        try {
            ApplicationInfo appInfo = getPackageManager().getApplicationInfo(
                    packageName, PackageManager.GET_META_DATA);
            Bundle bundle = appInfo.metaData;
            String url = bundle.getString(META_DATA_START_URL);

            Intent intent = getIntent();
            String overrideUrl = intent.getDataString();
            // TODO(pkotwicz): Use same logic as {@code IntentHandler#shouldIgnoreIntent()}
            if (overrideUrl != null && overrideUrl.startsWith("https:")) {
                url = overrideUrl;
            }
            int source = intent.getIntExtra(EXTRA_SOURCE, 0);

            String webappId = WebApkConstants.WEBAPK_ID_PREFIX + packageName;
            String runtimeHost = bundle.getString(META_DATA_RUNTIME_HOST);
            String shortName = (String) getPackageManager().getApplicationLabel(appInfo);
            // TODO(hanxi): find a neat solution to avoid encode/decode each time launch the
            // activity.
            Bitmap icon = BitmapFactory.decodeResource(getResources(), R.drawable.app_icon);
            String encodedIcon = encodeBitmapAsString(icon);
            String name = bundle.getString(META_DATA_NAME);
            String displayMode = bundle.getString(META_DATA_DISPLAY_MODE);
            String orientation = bundle.getString(META_DATA_ORIENTATION);
            long themeColor = getLongFromBundle(bundle, META_DATA_THEME_COLOR);
            long backgroundColor = getLongFromBundle(bundle, META_DATA_BACKGROUND_COLOR);
            boolean isIconGenerated = TextUtils.isEmpty(bundle.getString(META_DATA_ICON_URL));
            Log.v(TAG, "Url of the WebAPK: " + url);
            Log.v(TAG, "WebappId of the WebAPK: " + webappId);
            Log.v(TAG, "Name of the WebAPK:" + name);
            Log.v(TAG, "Package name of the WebAPK:" + packageName);

            Intent newIntent = new Intent();
            newIntent.setComponent(new ComponentName(runtimeHost,
                    "org.chromium.chrome.browser.webapps.WebappLauncherActivity"));
            // Chrome expects the ShortcutHelper.EXTRA_DISPLAY_MODE and
            // ShortcutHelper.EXTRA_ORIENTATION extras to be enum values. We send string extras for
            // the display mode and orientation so have to use different keys.
            newIntent.putExtra(EXTRA_ID, webappId)
                     .putExtra(EXTRA_SHORT_NAME, shortName)
                     .putExtra(EXTRA_NAME, name)
                     .putExtra(EXTRA_URL, url)
                     .putExtra(EXTRA_ICON, encodedIcon)
                     .putExtra(EXTRA_SOURCE, source)
                     .putExtra(EXTRA_THEME_COLOR, themeColor)
                     .putExtra(EXTRA_BACKGROUND_COLOR, backgroundColor)
                     .putExtra(EXTRA_IS_ICON_GENERATED, isIconGenerated)
                     .putExtra(EXTRA_WEBAPK_PACKAGE_NAME, packageName)
                     .putExtra(WebApkConstants.EXTRA_WEBAPK_DISPLAY_MODE, displayMode)
                     .putExtra(WebApkConstants.EXTRA_WEBAPK_ORIENTATION, orientation);
            startActivity(newIntent);
            finish();
        } catch (NameNotFoundException e) {
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
