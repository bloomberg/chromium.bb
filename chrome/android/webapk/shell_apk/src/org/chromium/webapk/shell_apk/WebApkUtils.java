// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;

import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.List;

/**
 * Contains utility methods for interacting with WebAPKs.
 */
public class WebApkUtils {
    private static final int MINIMUM_REQUIRED_CHROME_VERSION = 57;
    private static final String TAG = "cr_WebApkUtils";

    private static final float CONTRAST_LIGHT_ITEM_THRESHOLD = 3f;

    /** Returns whether the application is installed and enabled. */
    public static boolean isInstalled(PackageManager packageManager, String packageName) {
        if (TextUtils.isEmpty(packageName)) return false;

        ApplicationInfo info;
        try {
            info = packageManager.getApplicationInfo(packageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
        return info.enabled;
    }

    /** Returns the <meta-data> value in the Android Manifest for {@link key}. */
    public static String readMetaDataFromManifest(Context context, String key) {
        Bundle metadata = readMetaData(context);
        if (metadata == null) return null;

        return metadata.getString(key);
    }

    /** Returns the <meta-data> in the Android Manifest. */
    public static Bundle readMetaData(Context context) {
        ApplicationInfo ai = null;
        try {
            ai = context.getPackageManager().getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            return null;
        }
        return ai.metaData;
    }

    /**
     * Returns the new intent url, rewrite if |loggedIntentUrlParam| is set. A query parameter with
     * the original URL is appended to the URL. Note: if the intent url has been rewritten before,
     * we don't rewrite it again.
     */
    public static String rewriteIntentUrlIfNecessary(String intentStartUrl, Bundle metadata) {
        String startUrl = metadata.getString(WebApkMetaDataKeys.START_URL);
        String loggedIntentUrlParam =
                metadata.getString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM);

        if (TextUtils.isEmpty(loggedIntentUrlParam)) return intentStartUrl;

        if (intentStartUrl.startsWith(startUrl)
                && !TextUtils.isEmpty(
                           Uri.parse(intentStartUrl).getQueryParameter(loggedIntentUrlParam))) {
            return intentStartUrl;
        }

        Uri.Builder returnUrlBuilder = Uri.parse(startUrl).buildUpon();
        returnUrlBuilder.appendQueryParameter(loggedIntentUrlParam, intentStartUrl);
        return returnUrlBuilder.toString();
    }

    /** Returns a list of ResolveInfo for all of the installed browsers. */
    public static List<ResolveInfo> getInstalledBrowserResolveInfos(PackageManager packageManager) {
        Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://"));
        // Note: {@link PackageManager#queryIntentActivities()} does not return ResolveInfos for
        // disabled browsers.
        return packageManager.queryIntentActivities(browserIntent, PackageManager.MATCH_ALL);
    }

    /**
     * Android uses padding_left under API level 17 and uses padding_start after that.
     * If we set the padding in resource file, android will create duplicated resource xml
     * with the padding to be different.
     */
    public static void setPaddingInPixel(View view, int start, int top, int end, int bottom) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            view.setPaddingRelative(start, top, end, bottom);
        } else {
            view.setPadding(start, top, end, bottom);
        }
    }

    /**
     * Imitates Chrome's @style/AlertDialogContent. We set the style via Java instead of via
     * specifying the style in the XML to avoid having layout files in both layout-v17/ and in
     * layout/.
     */
    public static void applyAlertDialogContentStyle(
            Context context, View contentView, TextView titleView) {
        Resources res = context.getResources();
        titleView.setTextColor(getColor(res, R.color.black_alpha_87));
        titleView.setTextSize(
                TypedValue.COMPLEX_UNIT_PX, res.getDimension(R.dimen.headline_size_medium));
        int dialogContentPadding = res.getDimensionPixelSize(R.dimen.dialog_content_padding);
        int titleBottomPadding = res.getDimensionPixelSize(R.dimen.title_bottom_padding);
        setPaddingInPixel(titleView, dialogContentPadding, dialogContentPadding,
                dialogContentPadding, titleBottomPadding);

        int dialogContentTopPadding = res.getDimensionPixelSize(R.dimen.dialog_content_top_padding);
        setPaddingInPixel(contentView, dialogContentPadding, dialogContentTopPadding,
                dialogContentPadding, dialogContentPadding);
    }

    /**
     * @see android.content.res.Resources#getColor(int id).
     */
    @SuppressWarnings("deprecation")
    public static int getColor(Resources res, int id) throws Resources.NotFoundException {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return res.getColor(id, null);
        } else {
            return res.getColor(id);
        }
    }

    /** Returns whether a WebAPK should be launched as a tab. See crbug.com/772398. */
    public static boolean shouldLaunchInTab(String versionName) {
        int dotIndex = versionName.indexOf(".");
        if (dotIndex == -1) return false;

        int version = Integer.parseInt(versionName.substring(0, dotIndex));
        return version < MINIMUM_REQUIRED_CHROME_VERSION;
    }
}
