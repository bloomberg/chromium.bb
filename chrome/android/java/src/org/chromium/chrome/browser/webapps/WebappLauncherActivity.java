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
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.webapk.lib.client.WebApkValidator;

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
        WebappInfo webappInfo = WebappInfo.create(intent);

        // {@link WebappInfo#create()} returns null if the intent does not specify the id or the
        // uri.
        if (webappInfo == null) return;

        String webappUrl = webappInfo.uri().toString();
        String webApkPackageName = webappInfo.webApkPackageName();
        int webappSource = webappInfo.source();
        String webappMac = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_MAC);

        ChromeWebApkHost.init();
        boolean isValidWebApk = isValidWebApk(webApkPackageName, webappUrl);

        if (isValidWebApk) {
            // {@link #isValidWebApk} checks whether the start URL sent in the intent is in the
            // scope of a WebAPK but it does not check that the intent was sent from Chrome. Unlike
            // non-WebAPK web apps, WebAPK ids are predictable. A malicious actor may send an intent
            // with a valid start URL and arbitrary other data. Only use the start URL, the package
            // name and the ShortcutSource from the launch intent and extract the remaining data
            // from the <meta-data> in the WebAPK's Android manifest.
            webappInfo = WebApkMetaDataUtils.extractWebappInfoFromWebApk(
                    webApkPackageName, webappUrl, webappInfo.source());

            if (webappInfo == null) return;
        }

        // Permit the launch to a standalone web app frame if any of the following are true:
        // - the request was for a WebAPK that is valid;
        // - the MAC is present and valid for the homescreen shortcut to be opened;
        // - the intent was sent by Chrome.
        if (isValidWebApk || isValidMacForUrl(webappUrl, webappMac)
                || wasIntentFromChrome(intent)) {
            LaunchMetrics.recordHomeScreenLaunchIntoStandaloneActivity(webappUrl, webappSource);
            Intent launchIntent = createWebappLaunchIntent(webappInfo, webappSource, isValidWebApk);
            startActivity(launchIntent);
            return;
        }

        Log.e(TAG, "Shortcut (%s) opened in Chrome.", webappUrl);

        // The shortcut data doesn't match the current encoding. Change the intent action to
        // launch the URL with a VIEW Intent in the regular browser.
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
        return IntentHandler.wasIntentSenderChrome(intent, ContextUtils.getApplicationContext());
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

            // If this is launching from a notification, we want to ensure that the URL being
            // launched is the URL in the intent. If a paused WebappActivity exists for this id,
            // then by default it will be focused and we have no way of sending the desired URL to
            // it (the intent is swallowed). As a workaround, set the CLEAR_TOP flag to ensure that
            // the existing Activity is cleared and relaunched with this intent.
            // TODO(dominickn): ideally, we want be able to route an intent to
            // WebappActivity.onNewIntent instead of restarting the Activity.
            if (source == ShortcutSource.NOTIFICATION) {
                launchIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
            }
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
     * Checks whether the package being targeted is a valid WebAPK and whether the url supplied
     * can be fulfilled by that WebAPK.
     *
     * @param webApkPackage The package name of the requested WebAPK.
     * @param url The url to navigate to.
     * @return true iff all validation criteria are met.
     */
    private boolean isValidWebApk(String webApkPackage, String url) {
        if (webApkPackage == null || !ChromeWebApkHost.isEnabled()) {
            return false;
        }
        if (!webApkPackage.equals(WebApkValidator.queryWebApkPackage(this, url))) {
            Log.d(TAG, "%s is not within scope of %s WebAPK", url, webApkPackage);
            return false;
        }
        return true;
    }
}
