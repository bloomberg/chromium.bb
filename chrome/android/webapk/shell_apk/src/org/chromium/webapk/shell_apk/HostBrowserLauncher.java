// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.ArrayList;

/** Contains methods for launching host browser. */
public class HostBrowserLauncher {
    private static final String TAG = "cr_HostBrowserLauncher";

    // Action for launching {@link WebappLauncherActivity}.
    // TODO(hanxi): crbug.com/737556. Replaces this string with the new WebAPK launch action after
    // it is propagated to all the Chrome's channels.
    public static final String ACTION_START_WEBAPK =
            "com.google.android.apps.chrome.webapps.WebappManager.ACTION_START_WEBAPP";

    // Must stay in sync with
    // {@link org.chromium.chrome.browser.ShortcutHelper#REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB}.
    private static final String REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB =
            "REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB";

    private static void grantUriPermissionToHostBrowser(
            Context context, Intent launchIntent, String hostBrowserPackageName) {
        ArrayList<Uri> uris = launchIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
        if (uris == null) {
            uris = new ArrayList<>();
            Uri uri = launchIntent.getParcelableExtra(Intent.EXTRA_STREAM);
            if (uri != null) {
                uris.add(uri);
            }
        }
        for (Uri uri : uris) {
            context.grantUriPermission(
                    hostBrowserPackageName, uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
    }

    /**
     * Launches host browser in WebAPK mode if the browser is WebAPK-compatible.
     * Otherwise, launches the host browser in tabbed mode.
     */
    public static void launch(Context context, HostBrowserLauncherParams params) {
        Log.v(TAG, "WebAPK Launch URL: " + params.getStartUrl());

        if (HostBrowserUtils.shouldLaunchInTab(params.getHostBrowserPackageName(),
                    params.getHostBrowserMajorChromiumVersion())) {
            launchInTab(context, params);
            return;
        }

        launchBrowserInWebApkMode(context, params, null, Intent.FLAG_ACTIVITY_NEW_TASK);
    }

    /** Launches host browser in WebAPK mode. */
    public static void launchBrowserInWebApkMode(
            Context context, HostBrowserLauncherParams params, Bundle extraExtras, int flags) {
        if (params.getSelectedShareTargetActivityClassName() != null) {
            grantUriPermissionToHostBrowser(
                    context, params.getOriginalIntent(), params.getHostBrowserPackageName());
        }

        Intent intent = new Intent();
        intent.setAction(ACTION_START_WEBAPK);
        intent.setPackage(params.getHostBrowserPackageName());
        intent.setFlags(flags);

        Bundle copiedExtras = params.getOriginalIntent().getExtras();
        if (copiedExtras != null) {
            intent.putExtras(copiedExtras);
        }

        // {@link WebApkConstants.EXTRA_RELAUNCH} causes the browser the relaunch the WebAPK. Avoid
        // an infinite relaunch loop by explicity removing the extra and adding it back only if it
        // is in {@link extraExtras}.
        intent.removeExtra(WebApkConstants.EXTRA_RELAUNCH);

        intent.putExtra(WebApkConstants.EXTRA_URL, params.getStartUrl())
                .putExtra(WebApkConstants.EXTRA_SOURCE, params.getSource())
                .putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, context.getPackageName())
                .putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                        params.getSelectedShareTargetActivityClassName())
                .putExtra(WebApkConstants.EXTRA_FORCE_NAVIGATION, params.getForceNavigation());

        if (extraExtras != null) {
            intent.putExtras(extraExtras);
        }

        // Only pass on the start time if:
        // - The WebAPK is not already running.
        // - No user action was required between launching the webapk and chrome starting up.
        //   See https://crbug.com/842023
        if (!params.wasDialogShown() && params.getLaunchTimeMs() >= 0) {
            intent.putExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, params.getLaunchTimeMs());
        }

        try {
            context.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "Unable to launch browser in WebAPK mode.");
            e.printStackTrace();
        }
    }

    /** Launches a WebAPK in its runtime host browser as a tab. */
    private static void launchInTab(Context context, HostBrowserLauncherParams params) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(params.getStartUrl()));
        intent.setPackage(params.getHostBrowserPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true)
                .putExtra(WebApkConstants.EXTRA_SOURCE, params.getSource());
        try {
            context.startActivity(intent);
        } catch (ActivityNotFoundException e) {
        }
    }
}
