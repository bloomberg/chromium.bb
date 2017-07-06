// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.text.TextUtils;
import android.util.Base64;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.webapk.lib.client.WebApkValidator;
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
        super.onCreate(savedInstanceState);
        launchActivity();
        ApiCompatibilityUtils.finishAndRemoveTask(this);
    }

    public void launchActivity() {
        Intent intent = getIntent();

        ChromeWebApkHost.init();
        boolean validWebApk = isValidWebApk(intent);

        WebappInfo webappInfo;
        if (validWebApk) {
            webappInfo = WebApkInfo.create(intent);
        } else {
            webappInfo = WebappInfo.create(intent);
        }

        // {@link WebApkInfo#create()} and {@link WebappInfo#create()} return null if the intent
        // does not specify required values such as the uri.
        if (webappInfo == null) {
            String url = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_URL);
            launchInTab(url, ShortcutSource.UNKNOWN);
            return;
        }

        String webappUrl = webappInfo.uri().toString();
        int webappSource = webappInfo.source();
        String webappMac = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_MAC);

        // Permit the launch to a standalone web app frame if any of the following are true:
        // - the request was for a WebAPK that is valid;
        // - the MAC is present and valid for the homescreen shortcut to be opened;
        // - the intent was sent by Chrome.
        if (validWebApk || isValidMacForUrl(webappUrl, webappMac)
                || wasIntentFromChrome(intent)) {
            int source = webappSource;
            // Retrieves the source of the WebAPK from WebappDataStorage if it is unknown. The
            // {@link webappSource} will not be unknown in the case of an external intent or a
            // notification that launches a WebAPK. Otherwise, it's not trustworthy and we must read
            // the SharedPreference to get the installation source.
            if (validWebApk && (webappSource == ShortcutSource.UNKNOWN)) {
                source = getWebApkSource(webappInfo);
            }
            LaunchMetrics.recordHomeScreenLaunchIntoStandaloneActivity(webappUrl, source);
            Intent launchIntent = createWebappLaunchIntent(webappInfo, webappSource, validWebApk);
            startActivity(launchIntent);
            return;
        }

        Log.e(TAG, "Shortcut (%s) opened in Chrome.", webappUrl);

        // The shortcut data doesn't match the current encoding. Change the intent action to
        // launch the URL with a VIEW Intent in the regular browser.
        launchInTab(webappUrl, webappSource);
    }

    // Gets the source of a WebAPK from the WebappDataStorage if the source has been stored before.
    private int getWebApkSource(WebappInfo webappInfo) {
        WebappDataStorage storage = null;

        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            WebappRegistry.warmUpSharedPrefsForId(webappInfo.id());
            storage = WebappRegistry.getInstance().getWebappDataStorage(webappInfo.id());
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        if (storage != null) {
            int source = storage.getSource();
            if (source != ShortcutSource.UNKNOWN) {
                return source;
            }
        }
        return ShortcutSource.WEBAPK_UNKNOWN;
    }

    private void launchInTab(String webappUrl, int webappSource) {
        if (TextUtils.isEmpty(webappUrl)) return;

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(webappUrl));
        launchIntent.setClassName(getPackageName(), ChromeLauncherActivity.class.getName());
        launchIntent.putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        launchIntent.putExtra(ShortcutHelper.EXTRA_SOURCE, webappSource);
        launchIntent.setFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | ApiCompatibilityUtils.getActivityNewDocumentFlag());
        startActivity(launchIntent);
    }

    /**
     * Checks whether or not the MAC is present and valid for the web app shortcut.
     *
     * The MAC is used to prevent malicious apps from launching Chrome into a full screen
     * Activity for phishing attacks (among other reasons).
     *
     * @param url The URL for the web app.
     * @param mac MAC to compare the URL against.  See {@link WebappAuthenticator}.
     * @return Whether the MAC is valid for the URL.
     */
    private boolean isValidMacForUrl(String url, String mac) {
        return mac != null
                && WebappAuthenticator.isUrlValid(this, url, Base64.decode(mac, Base64.DEFAULT));
    }

    private boolean wasIntentFromChrome(Intent intent) {
        return IntentHandler.wasIntentSenderChrome(intent);
    }

    /**
     * Creates an Intent to launch the web app.
     * @param info     Information about the web app.
     * @param isWebApk If true, launch the app as a WebApkActivity.  If false, launch the app as
     *                 a WebappActivity.
     */
    private Intent createWebappLaunchIntent(WebappInfo info, int source, boolean isWebApk) {
        String activityName = isWebApk ? WebApkActivity.class.getName()
                : WebappActivity.class.getName();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            // Specifically assign the app to a particular WebappActivity instance.
            int namespace = isWebApk
                    ? ActivityAssigner.WEBAPK_NAMESPACE : ActivityAssigner.WEBAPP_NAMESPACE;
            int activityIndex = ActivityAssigner.instance(namespace).assign(info.id());
            activityName += String.valueOf(activityIndex);

            // Finishes the old activity if it has been assigned to a different WebappActivity. See
            // crbug.com/702998.
            for (WeakReference<Activity> activityRef : ApplicationStatus.getRunningActivities()) {
                Activity activity = activityRef.get();
                if (!(activity instanceof WebappActivity)
                        || !activity.getClass().getName().equals(activityName)) {
                    continue;
                }
                WebappActivity webappActivity = (WebappActivity) activity;
                if (!TextUtils.equals(webappActivity.mWebappInfo.id(), info.id())) {
                    activity.finish();
                }
                break;
            }
        }

        // Create an intent to launch the Webapp in an unmapped WebappActivity.
        Intent launchIntent = new Intent();
        launchIntent.setClassName(this, activityName);
        info.setWebappIntentExtras(launchIntent);

        // On L+, firing intents with the exact same data should relaunch a particular
        // Activity.
        launchIntent.setAction(Intent.ACTION_VIEW);
        launchIntent.setData(Uri.parse(WebappActivity.WEBAPP_SCHEME + "://" + info.id()));
        // Setting FLAG_ACTIVITY_CLEAR_TOP handles 2 edge cases:
        // - If a legacy PWA is launching from a notification, we want to ensure that the URL being
        // launched is the URL in the intent. If a paused WebappActivity exists for this id,
        // then by default it will be focused and we have no way of sending the desired URL to
        // it (the intent is swallowed). As a workaround, set the CLEAR_TOP flag to ensure that
        // the existing Activity handles an update via onNewIntent().
        // - If a WebAPK is having a CustomTabActivity on top of it in the same Task, and user
        // clicks a link to takes them back to the scope of a WebAPK, we want to destroy the
        // CustomTabActivity activity and go back to the WebAPK activity. It is intentional that
        // Custom Tab will not be reachable with a back button.
        launchIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | ApiCompatibilityUtils.getActivityNewDocumentFlag()
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);
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
     * Checks whether the WebAPK package specified in the intent is a valid WebAPK and whether the
     * url specified in the intent can be fulfilled by the WebAPK.
     *
     * @param intent The intent
     * @return true iff all validation criteria are met.
     */
    private boolean isValidWebApk(Intent intent) {
        String webApkPackage =
                IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);
        if (TextUtils.isEmpty(webApkPackage)) return false;

        String url = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_URL);
        if (TextUtils.isEmpty(url)) return false;

        if (!WebApkValidator.canWebApkHandleUrl(this, webApkPackage, url)) {
            Log.d(TAG, "%s is not within scope of %s WebAPK", url, webApkPackage);
            return false;
        }
        return true;
    }
}
