// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.TypedArray;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.webapps.WebApkInfo;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * This class makes a decision what FRE type to launch and creates a corresponding intent. Should be
 * instantiated using {@link AppHooks#createFreIntentCreator}.
 */
public class FreIntentCreator {
    private static final int FIRST_RUN_EXPERIENCE_REQUEST_CODE = 101;

    /**
     * Creates an intent to launch the First Run Experience.
     *
     * @param caller               Activity instance that is requesting the first run.
     * @param fromIntent           Intent used to launch the caller.
     * @param requiresBroadcast    Whether or not the Intent triggers a BroadcastReceiver.
     * @param preferLightweightFre Whether to prefer the Lightweight First Run Experience.
     * @return Intent to launch First Run Experience.
     */
    public Intent create(Context caller, Intent fromIntent, boolean requiresBroadcast,
            boolean preferLightweightFre) {
        @Nullable WebApkInfo webApkInfo =
                WebappLauncherActivity.maybeSlowlyGenerateWebApkInfoFromIntent(fromIntent);
        Intent intentToLaunchAfterFreComplete = (webApkInfo == null)
                ? fromIntent
                : WebappLauncherActivity.createRelaunchWebApkIntent(fromIntent, webApkInfo);

        // Launch the Generic First Run Experience if it was previously active.
        boolean isGenericFreActive = checkIsGenericFreActive();
        if (preferLightweightFre && !isGenericFreActive) {
            return createLightweightFirstRunIntent(caller, fromIntent, webApkInfo,
                    intentToLaunchAfterFreComplete, requiresBroadcast);
        } else {
            return createGenericFirstRunIntent(
                    caller, fromIntent, intentToLaunchAfterFreComplete, requiresBroadcast);
        }
    }

    /**
     * Returns an intent to show the lightweight first run activity.
     * @param context                        The context.
     * @param fromIntent                     The intent that was used to launch Chrome.
     * @param webApkInfo                     An optional WebApkInfo if this FRE flow was triggered
     *                                       by launching a WebAPK.
     * @param intentToLaunchAfterFreComplete The intent to launch when the user completes the FRE.
     * @param requiresBroadcast              Whether the relaunch intent must be broadcasted.
     */
    private static Intent createLightweightFirstRunIntent(Context context, Intent fromIntent,
            @Nullable WebApkInfo webApkInfo, Intent intentToLaunchAfterFreComplete,
            boolean requiresBroadcast) {
        Intent intent = new Intent();
        intent.setClassName(context, LightweightFirstRunActivity.class.getName());
        String webApkShortName = webApkInfo == null ? null : webApkInfo.shortName();
        if (webApkShortName != null) {
            intent.putExtra(LightweightFirstRunActivity.EXTRA_ASSOCIATED_APP_NAME, webApkShortName);
        }
        addPendingIntent(context, intent, intentToLaunchAfterFreComplete, requiresBroadcast);
        return intent;
    }

    /**
     * @return A generic intent to show the First Run Activity.
     * @param context                        The context.
     * @param fromIntent                     The intent that was used to launch Chrome.
     * @param intentToLaunchAfterFreComplete The intent to launch when the user completes the FRE.
     * @param requiresBroadcast              Whether the relaunch intent must be broadcasted.
     */
    private static Intent createGenericFirstRunIntent(Context context, Intent fromIntent,
            Intent intentToLaunchAfterFreComplete, boolean requiresBroadcast) {
        final Class<?> activityClass;
        if (shouldSwitchToTabbedMode(context)) {
            activityClass = TabbedModeFirstRunActivity.class;

            // Going to launch TabbedModeFRE. Have to disable animation on the intent launched after
            // the FRE is completed to make the transition seamless.
            intentToLaunchAfterFreComplete = new Intent(intentToLaunchAfterFreComplete);
            intentToLaunchAfterFreComplete.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        } else {
            activityClass = FirstRunActivity.class;
        }

        Intent intent = new Intent();
        intent.setClass(context, activityClass);
        intent.putExtra(FirstRunActivity.EXTRA_COMING_FROM_CHROME_ICON,
                TextUtils.equals(fromIntent.getAction(), Intent.ACTION_MAIN));
        intent.putExtra(FirstRunActivity.EXTRA_CHROME_LAUNCH_INTENT_IS_CCT,
                LaunchIntentDispatcher.isCustomTabIntent(fromIntent));
        // Copy extras bundle from intent which was used to launch Chrome. Copying the extras
        // enables the FirstRunActivity to locate the associated CustomTabsSession (if there
        // is one) and to notify the connection of whether the FirstRunActivity was completed.
        Bundle fromIntentExtras = fromIntent.getExtras();
        if (fromIntentExtras != null) {
            Bundle copiedFromExtras = new Bundle(fromIntentExtras);
            intent.putExtra(FirstRunActivity.EXTRA_CHROME_LAUNCH_INTENT_EXTRAS, copiedFromExtras);
        }

        addPendingIntent(context, intent, intentToLaunchAfterFreComplete, requiresBroadcast);

        return intent;
    }

    /**
     * Adds fromIntent as a PendingIntent to the firstRunIntent. This should be used to add a
     * PendingIntent that will be sent when first run is completed.
     *
     * @param context                        The context that corresponds to the Intent.
     * @param firstRunIntent                 The intent that will be used to start first run.
     * @param intentToLaunchAfterFreComplete The intent to launch when the user completes the FRE.
     * @param requiresBroadcast              Whether or not the fromIntent must be broadcasted.
     */
    private static void addPendingIntent(Context context, Intent firstRunIntent,
            Intent intentToLaunchAfterFreComplete, boolean requiresBroadcast) {
        PendingIntent pendingIntent = null;
        int pendingIntentFlags = PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_ONE_SHOT;
        if (requiresBroadcast) {
            pendingIntent = PendingIntent.getBroadcast(context, FIRST_RUN_EXPERIENCE_REQUEST_CODE,
                    intentToLaunchAfterFreComplete, pendingIntentFlags);
        } else {
            pendingIntent = PendingIntent.getActivity(context, FIRST_RUN_EXPERIENCE_REQUEST_CODE,
                    intentToLaunchAfterFreComplete, pendingIntentFlags);
        }
        firstRunIntent.putExtra(FirstRunActivity.EXTRA_FRE_COMPLETE_LAUNCH_INTENT, pendingIntent);
    }

    /** Returns whether the generic FRE is active. */
    private static boolean checkIsGenericFreActive() {
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            // TabbedModeFirstRunActivity extends FirstRunActivity. LightweightFirstRunActivity
            // does not.
            if (activity instanceof FirstRunActivity) {
                return true;
            }
        }
        return false;
    }

    /**
     * On tablets, where FRE activity is a dialog, transitions from fillscreen activities
     * (the ones that use Theme.Chromium.TabbedMode, e.g. ChromeTabbedActivity) look ugly, because
     * when FRE is started from CTA.onCreate(), currently running animation for CTA window
     * is aborted. This is perceived as a flash of white and doesn't look good.
     *
     * To solve this, we added TabbedMode FRE activity, which has the same window background
     * as Theme.Chromium.TabbedMode activities, but shows content in a FRE-like dialog.
     *
     * This function returns whether to use the TabbedModeFRE.
     */
    private static boolean shouldSwitchToTabbedMode(Context caller) {
        // Caller must be an activity.
        if (!(caller instanceof Activity)) return false;

        // We must be on a tablet (where FRE is a dialog).
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(caller)) return false;

        // Caller must use a theme with @drawable/window_background (the same background
        // used by TabbedModeFRE).
        TypedArray a = caller.obtainStyledAttributes(new int[] {android.R.attr.windowBackground});
        int backgroundResourceId = a.getResourceId(0 /* index */, 0);
        a.recycle();
        return (backgroundResourceId == org.chromium.chrome.R.drawable.window_background);
    }
}
