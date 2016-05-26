// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Base64;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.lang.ref.WeakReference;

/**
 * Launches web apps.  This was separated from the ChromeLauncherActivity because the
 * ChromeLauncherActivity is not allowed to be excluded from Android's Recents: crbug.com/517426.
 */
public class WebappLauncherActivity extends Activity {
    /**
     * Action fired when an Intent is trying to launch a WebappActivity.
     * Never change the package name or the Intents will fail to launch.
     */
    public static final String ACTION_START_WEBAPP =
            "com.google.android.apps.chrome.webapps.WebappManager.ACTION_START_WEBAPP";

    private static final String TAG = "webapps";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        WebappInfo webappInfo = WebappInfo.create(getIntent());
        if (webappInfo == null) {
            super.onCreate(null);
            ApiCompatibilityUtils.finishAndRemoveTask(this);
            return;
        }

        super.onCreate(savedInstanceState);
        Intent intent = getIntent();
        String webappId = webappInfo.id();
        String webappUrl = webappInfo.uri().toString();
        String webApkPackageName = webappInfo.webApkPackageName();
        int webappSource = webappInfo.source();

        if (webappId != null && webappUrl != null) {
            String webappMacString = IntentUtils.safeGetStringExtra(
                    intent, ShortcutHelper.EXTRA_MAC);
            byte[] webappMac =
                    webappMacString == null ? null : Base64.decode(webappMacString, Base64.DEFAULT);

            Intent launchIntent = null;

            // Permit the launch to a standalone web app frame if the intent was sent by Chrome, or
            // if the MAC is present and valid for the URL to be opened.
            boolean isTrusted = IntentHandler.wasIntentSenderChrome(intent,
                    ContextUtils.getApplicationContext());
            boolean isUrlValid = (webappMac != null
                    && WebappAuthenticator.isUrlValid(this, webappUrl, webappMac));
            boolean isValidWebApk = isValidWebApk(webApkPackageName);
            if (webApkPackageName != null && !isValidWebApk) {
                isUrlValid = false;
            }

            if (isTrusted || isUrlValid) {
                LaunchMetrics.recordHomeScreenLaunchIntoStandaloneActivity(webappUrl, webappSource);
                launchIntent = createWebappLaunchIntent(webappInfo, isValidWebApk);
            } else {
                Log.e(TAG, "Shortcut (%s) opened in Chrome.", webappUrl);

                // The shortcut data doesn't match the current encoding.  Change the intent action
                // launch the URL with a VIEW Intent in the regular browser.
                launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(webappUrl));
                launchIntent.setClassName(getPackageName(), ChromeLauncherActivity.class.getName());
                launchIntent.putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
                launchIntent.putExtra(ShortcutHelper.EXTRA_SOURCE, webappSource);
                launchIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                        | ApiCompatibilityUtils.getActivityNewDocumentFlag());
            }

            startActivity(launchIntent);
        }

        ApiCompatibilityUtils.finishAndRemoveTask(this);
    }

    /**
     * Creates an Intent to launch the web app.
     * @param info     Information about the web app.
     * @param isWebApk If true, launch the app as a WebApkActivity.  If false, launch the app as
     *                 a WebappActivity.
     */
    private Intent createWebappLaunchIntent(WebappInfo info, boolean isWebApk) {
        String activityName = isWebApk ? WebApkActivity.class.getName()
                : WebappActivity.class.getName();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            // Specifically assign the app to a particular WebappActivity instance.
            int activityIndex = ActivityAssigner.instance(info.id()).assign(info.id());
            activityName += String.valueOf(activityIndex);
        }

        // Create an intent to launch the Webapp in an unmapped WebappActivity.
        Intent launchIntent = new Intent();
        launchIntent.setClassName(this, activityName);
        info.setWebappIntentExtras(launchIntent);

        // On L+, firing intents with the exact same data should relaunch a particular
        // Activity.
        launchIntent.setAction(Intent.ACTION_VIEW);
        launchIntent.setData(Uri.parse(WebappActivity.WEBAPP_SCHEME + "://" + info.id()));

        if (!isWebApk) {
            // For WebAPK, we don't start a new task for WebApkActivity, it is just on top
            // of the WebAPK's main activity and in the same task.
            launchIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                    | ApiCompatibilityUtils.getActivityNewDocumentFlag());
        }
        return launchIntent;
    }

    /**
     * Brings a live WebappActivity back to the foreground if one exists for the given tab ID.
     * @param tabId ID of the Tab to bring back to the foreground.
     * @return True if a live WebappActivity was found, false otherwise.
     */
    public static boolean bringWebappToFront(int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return false;

        for (WeakReference<Activity> activityRef : ApplicationStatus.getRunningActivities()) {
            Activity activity = activityRef.get();
            if (activity == null || !(activity instanceof WebappActivity)) continue;

            WebappActivity webappActivity = (WebappActivity) activity;
            if (webappActivity.getActivityTab() != null
                    && webappActivity.getActivityTab().getId() == tabId) {
                Tab tab = webappActivity.getActivityTab();
                tab.getTabWebContentsDelegateAndroid().activateContents();
                return true;
            }
        }

        return false;
    }

    /**
     * Checks whether the package being targeted is a valid WebAPK.
     * @param webapkPackageName The package name of the requested WebAPK.
     * @return true iff all validation criteria are met.
     */
    private boolean isValidWebApk(String webapkPackageName) {
        // TODO(hanxi): Adds more validation checks. For example, whether the WebAPK is signed
        // by the WebAPK Minting Server.
        return CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_WEBAPK)
                && webapkPackageName != null
                && webapkPackageName.startsWith(WebApkConstants.WEBAPK_PACKAGE_PREFIX);
    }
}
