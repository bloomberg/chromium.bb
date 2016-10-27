// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

/**
 * Contains methods for extracting meta data from WebAPK.
 */
public class WebApkMetaDataUtils {
    /**
     * Populates {@link WebappInfo} with meta data extracted from WebAPK.
     * @param webApkPackageName Package name of the WebAPK to extract meta data from.
     * @param url WebAPK start URL.
     * @param source {@link ShortcutSource} that the WebAPK was opened from.
     */
    public static WebappInfo extractWebappInfoFromWebApk(
            String webApkPackageName, String url, int source) {
        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        ApplicationInfo appInfo;
        try {
            appInfo = packageManager.getApplicationInfo(
                    webApkPackageName, PackageManager.GET_META_DATA);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
        Bundle metaData = appInfo.metaData;
        String scope = IntentUtils.safeGetString(metaData, WebApkMetaDataKeys.SCOPE);
        String name = IntentUtils.safeGetString(metaData, WebApkMetaDataKeys.NAME);
        String shortName = IntentUtils.safeGetString(metaData, WebApkMetaDataKeys.SHORT_NAME);
        int displayMode = displayModeFromString(
                IntentUtils.safeGetString(metaData, WebApkMetaDataKeys.DISPLAY_MODE));
        int orientation = orientationFromString(
                IntentUtils.safeGetString(metaData, WebApkMetaDataKeys.ORIENTATION));
        long themeColor = getLongFromMetaData(metaData, WebApkMetaDataKeys.THEME_COLOR,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        long backgroundColor = getLongFromMetaData(metaData, WebApkMetaDataKeys.BACKGROUND_COLOR,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        boolean isIconGenerated =
                TextUtils.isEmpty(IntentUtils.safeGetString(metaData, WebApkMetaDataKeys.ICON_URL));

        int iconId = IntentUtils.safeGetInt(metaData, WebApkMetaDataKeys.ICON_ID, 0);
        Resources resources;
        try {
            resources = packageManager.getResourcesForApplication(webApkPackageName);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
        Bitmap icon = BitmapFactory.decodeResource(resources, iconId);
        String encodedIcon = ShortcutHelper.encodeBitmapAsString(icon);

        return WebappInfo.create(WebApkConstants.WEBAPK_ID_PREFIX + webApkPackageName, url, scope,
                encodedIcon, name, shortName, displayMode, orientation, source, themeColor,
                backgroundColor, isIconGenerated, webApkPackageName);
    }

    /**
     * Extracts long value from the WebAPK's meta data.
     * @param metaData WebAPK meta data to extract the long from.
     * @param name Name of the <meta-data> tag to extract the value from.
     * @param defaultValue Value to return if long value could not be extracted.
     * @return long value.
     */
    public static long getLongFromMetaData(Bundle metaData, String name, long defaultValue) {
        String value = metaData.getString(name);

        // The value should be terminated with 'L' to force the value to be a string. According to
        // https://developer.android.com/guide/topics/manifest/meta-data-element.html numeric
        // meta data values can only be retrieved via {@link Bundle#getInt()} and
        // {@link Bundle#getFloat()}. We cannot use {@link Bundle#getFloat()} due to loss of
        // precision.
        if (value == null || !value.endsWith("L")) {
            return defaultValue;
        }
        try {
            return Long.parseLong(value.substring(0, value.length() - 1));
        } catch (NumberFormatException e) {
        }
        return defaultValue;
    }

    /**
     * Extracts icon murmur2 hash from the WebAPK's meta data. Return value is a string because the
     * hash can take values up to 2^64-1 which is greater than {@link Long#MAX_VALUE}.
     * @param metaData WebAPK meta data to extract the hash from.
     * @return The hash. An empty string if the hash could not be extracted.
     */
    public static String getIconMurmur2HashFromMetaData(Bundle metaData) {
        String value = metaData.getString(WebApkMetaDataKeys.ICON_MURMUR2_HASH);

        // The value should be terminated with 'L' to force the value to be a string.
        if (value == null || !value.endsWith("L")) {
            return "";
        }
        return value.substring(0, value.length() - 1);
    }

    /**
     * Returns the WebDisplayMode which matches {@link displayMode}.
     * @param displayMode One of https://www.w3.org/TR/appmanifest/#dfn-display-modes-values
     * @return The matching WebDisplayMode. {@link WebDisplayMode#Undefined} if there is no match.
     */
    private static int displayModeFromString(String displayMode) {
        if (displayMode == null) {
            return WebDisplayMode.Undefined;
        }

        if (displayMode.equals("fullscreen")) {
            return WebDisplayMode.Fullscreen;
        } else if (displayMode.equals("standalone")) {
            return WebDisplayMode.Standalone;
        } else if (displayMode.equals("minimal-ui")) {
            return WebDisplayMode.MinimalUi;
        } else if (displayMode.equals("browser")) {
            return WebDisplayMode.Browser;
        } else {
            return WebDisplayMode.Undefined;
        }
    }

    /**
     * Returns the ScreenOrientationValue which matches {@link orientation}.
     * @param orientation One of https://w3c.github.io/screen-orientation/#orientationlocktype-enum
     * @return The matching ScreenOrientationValue. {@link ScreenOrientationValues#DEFAULT} if there
     * is no match.
     */
    private static int orientationFromString(String orientation) {
        if (orientation == null) {
            return ScreenOrientationValues.DEFAULT;
        }

        if (orientation.equals("any")) {
            return ScreenOrientationValues.ANY;
        } else if (orientation.equals("natural")) {
            return ScreenOrientationValues.NATURAL;
        } else if (orientation.equals("landscape")) {
            return ScreenOrientationValues.LANDSCAPE;
        } else if (orientation.equals("landscape-primary")) {
            return ScreenOrientationValues.LANDSCAPE_PRIMARY;
        } else if (orientation.equals("landscape-secondary")) {
            return ScreenOrientationValues.LANDSCAPE_SECONDARY;
        } else if (orientation.equals("portrait")) {
            return ScreenOrientationValues.PORTRAIT;
        } else if (orientation.equals("portrait-primary")) {
            return ScreenOrientationValues.PORTRAIT_PRIMARY;
        } else if (orientation.equals("portrait-secondary")) {
            return ScreenOrientationValues.PORTRAIT_SECONDARY;
        } else {
            return ScreenOrientationValues.DEFAULT;
        }
    }
}
