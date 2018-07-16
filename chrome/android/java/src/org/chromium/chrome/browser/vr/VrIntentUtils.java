// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.Display;

import org.chromium.chrome.R;

/**
 * Utilities dealing with extracting information about VR intents.
 */
public class VrIntentUtils {
    // The Daydream Home app adds this extra to auto-present intents.
    public static final String AUTOPRESENT_WEVBVR_EXTRA = "browser.vr.AUTOPRESENT_WEBVR";
    public static final String DAYDREAM_CATEGORY = "com.google.intent.category.DAYDREAM";
    // Tells Chrome not to relaunch itself when receiving a VR intent. This is used by tests since
    // the relaunch logic does not work properly with the DON flow skipped.
    public static final String AVOID_RELAUNCH_EXTRA =
            "org.chromium.chrome.browser.vr.AVOID_RELAUNCH";
    // Tells Chrome to attempts a relaunch of the intent if it is received outside of VR and doesn't
    // have the Daydream category set. This is a workaround for https://crbug.com/854327 where
    // launchInVr can sometimes launch the given intent before entering VR.
    public static final String ENABLE_TEST_RELAUNCH_WORKAROUND_EXTRA =
            "org.chromium.chrome.browser.vr.ENABLE_TEST_RELUANCH_WORKAROUND";

    static final String VR_FRE_INTENT_EXTRA = "org.chromium.chrome.browser.vr.VR_FRE";

    /**
     * @return Whether or not the given intent is a VR-specific intent.
     */
    public static boolean isVrIntent(Intent intent) {
        // For simplicity, we only return true here if VR is enabled on the platform and this intent
        // is not fired from a recent apps page. The latter is there so that we don't enter VR mode
        // when we're being resumed from the recent apps in 2D mode.
        // Note that Daydream removes the Daydream category for deep-links (for no real reason). In
        // addition to the category, DAYDREAM_VR_EXTRA tells us that this intent is coming directly
        // from VR.
        return intent != null && intent.hasCategory(DAYDREAM_CATEGORY)
                && !launchedFromRecentApps(intent) && VrShellDelegate.isVrEnabled();
    }

    /**
     * @param activity The Activity to check.
     * @param intent The intent the Activity was launched with.
     * @return Whether this Activity is launching into VR.
     */
    public static boolean isLaunchingIntoVr(Activity activity, Intent intent) {
        if (!VrShellDelegate.deviceSupportsVrLaunches()) return false;
        return isLaunchingIntoVrBrowsing(activity, intent);
    }

    private static boolean isLaunchingIntoVrBrowsing(Activity activity, Intent intent) {
        return isVrIntent(intent) && VrShellDelegate.activitySupportsVrBrowsing(activity);
    }

    /**
     * This function returns an intent that will launch a VR activity that will prompt the
     * user to take off their headset and forward the freIntent to the standard
     * 2D FRE activity.
     *
     * @param freIntent       The intent that will be used to start the first run in 2D mode.
     * @return The intermediate VR activity intent.
     */
    public static Intent setupVrFreIntent(Context context, Intent freIntent) {
        if (!VrShellDelegate.isVrEnabled()) return freIntent;
        Intent intent = new Intent();
        intent.setClassName(context, VrFirstRunActivity.class.getName());
        intent.addCategory(DAYDREAM_CATEGORY);
        intent.putExtra(VR_FRE_INTENT_EXTRA, new Intent(freIntent));
        return intent;
    }

    /**
     * @return Options that a VR-specific Chrome activity should be launched with.
     */
    public static Bundle getVrIntentOptions(Context context) {
        if (!VrShellDelegate.isVrEnabled()) return null;
        // These options are used to start the Activity with a custom animation to keep it hidden
        // for a few hundred milliseconds - enough time for us to draw the first black view.
        // The animation is sufficient to hide the 2D screenshot but not to the 2D UI while the
        // WebVR page is being loaded because the animation is somehow cancelled when we try to
        // enter VR (I don't know what's canceling it). To hide the 2D UI, we resort to the black
        // overlay view added in {@link startWithVrIntentPreNative}.
        int animation = VrShellDelegate.USE_HIDE_ANIMATION ? R.anim.stay_hidden : 0;
        ActivityOptions options = ActivityOptions.makeCustomAnimation(context, animation, 0);
        if (VrShellDelegate.getVrClassesWrapper().bootsToVr()) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
                assert false;
            } else {
                options.setLaunchDisplayId(Display.DEFAULT_DISPLAY);
            }
        }
        return options.toBundle();
    }

    /**
     * @param intent The intent to launch in VR after going through the DON (Device On) flow.
     * @param activity The activity context to launch the intent from.
     */
    public static void launchInVr(Intent intent, Activity activity) {
        VrShellDelegate.getVrDaydreamApi().launchInVr(intent);
    }

    /**
     * @return Whether the intent is fired from the recent apps overview.
     */
    /* package */ static boolean launchedFromRecentApps(Intent intent) {
        return ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0);
    }

    /**
     * Removes VR specific extras from the given intent to make it a non-VR intent.
     */
    public static void removeVrExtras(Intent intent) {
        if (intent == null) return;
        intent.removeCategory(DAYDREAM_CATEGORY);
        assert !isVrIntent(intent);
    }

    /**
     * Adds the necessary VR flags to an intent.
     * @param intent The intent to add VR flags to.
     * @return the intent with VR flags set.
     */
    public static Intent setupVrIntent(Intent intent) {
        return VrShellDelegate.getVrClassesWrapper().setupVrIntent(intent);
    }
}
