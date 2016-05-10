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
import android.util.Base64;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkConstants;

import java.io.ByteArrayOutputStream;

/**
 * WebAPK's main Activity.
 */
public class MainActivity extends Activity {
    private static final String EXTRA_ID = "org.chromium.chrome.browser.webapp_id";
    private static final String EXTRA_ICON = "org.chromium.chrome.browser.webapp_icon";
    private static final String EXTRA_NAME = "org.chromium.chrome.browser.webapp_name";
    private static final String EXTRA_URL = "org.chromium.chrome.browser.webapp_url";
    private static final String EXTRA_MAC = "org.chromium.chrome.browser.webapp_mac";
    private static final String EXTRA_SCOPE = "org.chromium.chrome.browser.webapp_scope";
    private static final String META_DATA_HOST_URL = "hostUrl";
    private static final String META_DATA_MAC = "mac";
    private static final String META_DATA_SCOPE = "scope";
    private static final String META_DATA_RUNTIME_HOST = "runtimeHost";

    private static final String TAG = "cr_MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String packageName = getPackageName();
        String webappId = null;
        String mac = null;
        String name = null;
        String url = null;
        String scope = null;
        String encodedIcon = null;
        String runtimeHost = null;
        try {
            ApplicationInfo appInfo = getPackageManager().getApplicationInfo(
                    packageName, PackageManager.GET_META_DATA);
            Bundle bundle = appInfo.metaData;
            url = bundle.getString(META_DATA_HOST_URL);
            scope = bundle.getString(META_DATA_SCOPE);
            webappId = WebApkConstants.WEBAPK_ID_PREFIX + packageName;
            mac = bundle.getString(META_DATA_MAC);
            runtimeHost = bundle.getString(META_DATA_RUNTIME_HOST);
            name = (String) getPackageManager().getApplicationLabel(appInfo);
            // TODO(hanxi): find a neat solution to avoid encode/decode each time launch the
            // activity.
            Bitmap icon = BitmapFactory.decodeResource(getResources(), R.drawable.app_icon);
            encodedIcon = encodeBitmapAsString(icon);
            Log.w(TAG, "Url of the WebAPK: " + url);
            Log.w(TAG, "webappId of the WebAPK: " + webappId);
            Log.w(TAG, "name of the WebAPK:" + name);
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }

        Intent newIntent = new Intent();
        newIntent.setComponent(new ComponentName(runtimeHost,
                "org.chromium.chrome.browser.webapps.WebappLauncherActivity"));
        newIntent.putExtra(EXTRA_ID, webappId);
        newIntent.putExtra(EXTRA_NAME, name);
        newIntent.putExtra(EXTRA_SCOPE, scope);
        newIntent.putExtra(EXTRA_URL, url);
        newIntent.putExtra(EXTRA_MAC, mac);
        newIntent.putExtra(EXTRA_ICON, encodedIcon);
        startActivity(newIntent);
        finish();
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
}
