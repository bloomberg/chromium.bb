// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Base64;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.BookmarkUtils;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.WebappAuthenticator;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.common.ScreenOrientationValues;

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

    private static final String TAG = "cr.webapps";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent intent = getIntent();
        String webappId = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_ID);
        String webappUrl = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_URL);
        String webappIcon = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_ICON);
        int webappOrientation = IntentUtils.safeGetIntExtra(intent,
                ShortcutHelper.EXTRA_ORIENTATION, ScreenOrientationValues.DEFAULT);
        int webappSource = IntentUtils.safeGetIntExtra(intent,
                ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN);

        String webappName = WebappInfo.nameFromIntent(intent);
        String webappShortName = WebappInfo.shortNameFromIntent(intent);

        if (webappId != null && webappUrl != null) {
            String webappMacString = IntentUtils.safeGetStringExtra(
                    intent, ShortcutHelper.EXTRA_MAC);
            byte[] webappMac =
                    webappMacString == null ? null : Base64.decode(webappMacString, Base64.DEFAULT);

            Intent launchIntent = null;
            if (webappMac != null && WebappAuthenticator.isUrlValid(this, webappUrl, webappMac)) {
                LaunchMetrics.recordHomeScreenLaunchIntoStandaloneActivity(webappUrl, webappSource);
                launchIntent = createWebappIntent(webappId, webappUrl, webappIcon, webappName,
                        webappShortName, webappOrientation, webappSource);
            } else {
                Log.e(TAG, "Shortcut (" + webappUrl + ") opened in Chrome.");

                // The shortcut data doesn't match the current encoding.  Change the intent action
                // launch the URL with a VIEW Intent in the regular browser.
                launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(webappUrl));
                launchIntent.setClassName(getPackageName(), ChromeLauncherActivity.class.getName());
                launchIntent.putExtra(BookmarkUtils.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
                launchIntent.putExtra(ShortcutHelper.EXTRA_SOURCE, webappSource);
            }

            launchIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                    | ApiCompatibilityUtils.getActivityNewDocumentFlag());
            startActivity(launchIntent);
        }

        ApiCompatibilityUtils.finishAndRemoveTask(this);
    }

    /**
     * Creates an Intent that launches a WebappActivity for the given data.
     * @param id          ID of the webapp.
     * @param url         URL for the webapp.
     * @param icon        Base64 encoded Bitmap representing the webapp.
     * @param name        String to show on the splash screen.
     * @param shortName   String to show on the recents menu
     * @param orientation Default orientation for the activity.
     * @return Intent that can be used to launch the releveant WebappActivity.
     */
    private Intent createWebappIntent(String id, String url, String icon, String name,
            String shortName, int orientation, int source) {
        String activityName = WebappActivity.class.getName();
        if (!FeatureUtilities.isDocumentModeEligible(this)) {
            // Specifically assign the app to a particular WebappActivity instance.
            int activityIndex = ActivityAssigner.instance(this).assign(id);
            activityName += String.valueOf(activityIndex);
        }

        // Fire an intent to launch the Webapp in an unmapped Activity.
        Intent webappIntent = new Intent();
        webappIntent.setClassName(this, activityName);
        webappIntent.putExtra(ShortcutHelper.EXTRA_ICON, icon);
        webappIntent.putExtra(ShortcutHelper.EXTRA_ID, id);
        webappIntent.putExtra(ShortcutHelper.EXTRA_URL, url);
        webappIntent.putExtra(ShortcutHelper.EXTRA_NAME, name);
        webappIntent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, shortName);
        webappIntent.putExtra(ShortcutHelper.EXTRA_ORIENTATION, orientation);
        webappIntent.putExtra(ShortcutHelper.EXTRA_SOURCE, source);

        // On L, firing intents with the exact same data should relaunch a particular Activity.
        webappIntent.setAction(Intent.ACTION_VIEW);
        webappIntent.setData(Uri.parse(WebappActivity.WEBAPP_SCHEME + "://" + id));

        return webappIntent;
    }
}
