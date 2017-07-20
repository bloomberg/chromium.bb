// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.os.UserManager;
import android.speech.RecognizerIntent;
import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.firstrun.FirstRunGlueImpl;
import org.chromium.chrome.browser.omnibox.OmniboxPlaceholderFieldTrial;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.tabmodel.DocumentModeAssassin;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/**
 * A utility {@code class} meant to help determine whether or not certain features are supported by
 * this device.
 */
public class FeatureUtilities {
    private static final String TAG = "FeatureUtilities";
    private static final String HERB_EXPERIMENT_NAME = "TabManagementExperiment";
    private static final String HERB_EXPERIMENT_FLAVOR_PARAM = "type";

    private static Boolean sHasGoogleAccountAuthenticator;
    private static Boolean sHasRecognitionIntentHandler;
    private static Boolean sChromeHomeEnabled;
    private static String sChromeHomeSwipeLogicType;

    private static String sCachedHerbFlavor;
    private static boolean sIsHerbFlavorCached;

    /** Used to track if cached command line flags should be refreshed. */
    private static CommandLine.ResetListener sResetListener;

    /**
     * Determines whether or not the {@link RecognizerIntent#ACTION_WEB_SEARCH} {@link Intent}
     * is handled by any {@link android.app.Activity}s in the system.  The result will be cached for
     * future calls.  Passing {@code false} to {@code useCachedValue} will force it to re-query any
     * {@link android.app.Activity}s that can process the {@link Intent}.
     * @param context        The {@link Context} to use to check to see if the {@link Intent} will
     *                       be handled.
     * @param useCachedValue Whether or not to use the cached value from a previous result.
     * @return {@code true} if recognition is supported.  {@code false} otherwise.
     */
    public static boolean isRecognitionIntentPresent(Context context, boolean useCachedValue) {
        ThreadUtils.assertOnUiThread();
        if (sHasRecognitionIntentHandler == null || !useCachedValue) {
            PackageManager pm = context.getPackageManager();
            List<ResolveInfo> activities = pm.queryIntentActivities(
                    new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH), 0);
            sHasRecognitionIntentHandler = activities.size() > 0;
        }

        return sHasRecognitionIntentHandler;
    }

    /**
     * Determines whether or not the user has a Google account (so we can sync) or can add one.
     * @param context The {@link Context} that we should check accounts under.
     * @return Whether or not sync is allowed on this device.
     */
    public static boolean canAllowSync(Context context) {
        return (hasGoogleAccountAuthenticator(context) && hasSyncPermissions(context))
                || hasGoogleAccounts(context);
    }

    @VisibleForTesting
    static boolean hasGoogleAccountAuthenticator(Context context) {
        if (sHasGoogleAccountAuthenticator == null) {
            AccountManagerFacade accountHelper = AccountManagerFacade.get();
            sHasGoogleAccountAuthenticator = accountHelper.hasGoogleAccountAuthenticator();
        }
        return sHasGoogleAccountAuthenticator;
    }

    @VisibleForTesting
    static boolean hasGoogleAccounts(Context context) {
        return AccountManagerFacade.get().hasGoogleAccounts();
    }

    @SuppressLint("InlinedApi")
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private static boolean hasSyncPermissions(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return true;

        UserManager manager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        Bundle userRestrictions = manager.getUserRestrictions();
        return !userRestrictions.getBoolean(UserManager.DISALLOW_MODIFY_ACCOUNTS, false);
    }

    /**
     * Check whether Chrome should be running on document mode.
     * @param context The context to use for checking configuration.
     * @return Whether Chrome should be running on document mode.
     */
    public static boolean isDocumentMode(Context context) {
        return isDocumentModeEligible(context) && !DocumentModeAssassin.isOptedOutOfDocumentMode();
    }

    /**
     * Whether the device could possibly run in Document mode (may return true even if the document
     * mode is turned off).
     *
     * This function can't be changed to return false (even if document mode is deleted) because we
     * need to know whether a user needs to be migrated away.
     *
     * @param context The context to use for checking configuration.
     * @return Whether the device could possibly run in Document mode.
     */
    public static boolean isDocumentModeEligible(Context context) {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                && !DeviceFormFactor.isTablet();
    }

    /**
     * Records the current custom tab visibility state with native-side feature utilities.
     * @param visible Whether a custom tab is visible.
     */
    public static void setCustomTabVisible(boolean visible) {
        nativeSetCustomTabVisible(visible);
    }

    /**
     * Records whether the activity is in multi-window mode with native-side feature utilities.
     * @param isInMultiWindowMode Whether the activity is in Android N multi-window mode.
     */
    public static void setIsInMultiWindowMode(boolean isInMultiWindowMode) {
        nativeSetIsInMultiWindowMode(isInMultiWindowMode);
    }

    private static boolean isHerbDisallowed(Context context) {
        return isDocumentMode(context);
    }

    /**
     * @return Which flavor of Herb is being tested.
     *         See {@link ChromeSwitches#HERB_FLAVOR_ELDERBERRY} and its related switches.
     */
    public static String getHerbFlavor() {
        Context context = ContextUtils.getApplicationContext();
        if (isHerbDisallowed(context)) return ChromeSwitches.HERB_FLAVOR_DISABLED;

        if (!sIsHerbFlavorCached) {
            sCachedHerbFlavor = null;

            // Allowing disk access for preferences while prototyping.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            try {
                sCachedHerbFlavor = ChromePreferenceManager.getInstance().getCachedHerbFlavor();
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }

            sIsHerbFlavorCached = true;
            Log.d(TAG, "Retrieved cached Herb flavor: " + sCachedHerbFlavor);
        }

        return sCachedHerbFlavor;
    }

    /**
     * Caches flags that must take effect on startup but are set via native code.
     */
    public static void cacheNativeFlags() {
        cacheHerbFlavor();
        cacheChromeHomeEnabled();
        FirstRunGlueImpl.cacheFirstRunPrefs();
        OmniboxPlaceholderFieldTrial.cacheOmniboxPlaceholderGroup();

        // Propagate DONT_PREFETCH_LIBRARIES feature value to LibraryLoader. This can't
        // be done in LibraryLoader itself because it lives in //base and can't depend
        // on ChromeFeatureList.
        LibraryLoader.setDontPrefetchLibrariesOnNextRuns(
                ChromeFeatureList.isEnabled(ChromeFeatureList.DONT_PREFETCH_LIBRARIES));
    }

    /**
     * Caches which flavor of Herb the user prefers from native.
     */
    private static void cacheHerbFlavor() {
        Context context = ContextUtils.getApplicationContext();
        if (isHerbDisallowed(context)) return;

        String oldFlavor = getHerbFlavor();

        // Check the experiment value before the command line to put the user in the correct group.
        // The first clause does the null checks so so we can freely use the startsWith() function.
        String newFlavor = FieldTrialList.findFullName(HERB_EXPERIMENT_NAME);
        Log.d(TAG, "Experiment flavor: " + newFlavor);
        if (!TextUtils.isEmpty(newFlavor)
                && newFlavor.startsWith(ChromeSwitches.HERB_FLAVOR_ELDERBERRY)) {
            newFlavor = ChromeSwitches.HERB_FLAVOR_ELDERBERRY;
        } else {
            newFlavor = ChromeSwitches.HERB_FLAVOR_DISABLED;
        }

        CommandLine instance = CommandLine.getInstance();
        if (instance.hasSwitch(ChromeSwitches.HERB_FLAVOR_DISABLED_SWITCH)) {
            newFlavor = ChromeSwitches.HERB_FLAVOR_DISABLED;
        } else if (instance.hasSwitch(ChromeSwitches.HERB_FLAVOR_ELDERBERRY_SWITCH)) {
            newFlavor = ChromeSwitches.HERB_FLAVOR_ELDERBERRY;
        }

        Log.d(TAG, "Caching flavor: " + newFlavor);
        sCachedHerbFlavor = newFlavor;

        if (!TextUtils.equals(oldFlavor, newFlavor)) {
            ChromePreferenceManager.getInstance().setCachedHerbFlavor(newFlavor);
        }
    }

    /**
     * @return True if tab model merging for Android N+ is enabled.
     */
    public static boolean isTabModelMergingEnabled() {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)) {
            return false;
        }
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.M;
    }

    /**
     * Cache whether or not Chrome Home is enabled.
     */
    public static void cacheChromeHomeEnabled() {
        // Chrome Home doesn't work with tablets.
        if (DeviceFormFactor.isTablet()) return;

        boolean isChromeHomeEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_HOME);
        ChromePreferenceManager manager = ChromePreferenceManager.getInstance();
        manager.setChromeHomeEnabled(isChromeHomeEnabled);
    }

    /**
     * @return Whether or not chrome should attach the toolbar to the bottom of the screen.
     */
    public static boolean isChromeHomeEnabled() {
        if (sChromeHomeEnabled == null) {
            // Allow disk access for preferences while Chrome Home is in experimentation.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            try {
                sChromeHomeEnabled = ChromePreferenceManager.getInstance().isChromeHomeEnabled();
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }

            // If the browser has been initialized by this point, check the experiment as well to
            // avoid the restart logic in cacheChromeHomeEnabled.
            if (ChromeFeatureList.isInitialized()) {
                boolean chromeHomeExperimentEnabled =
                        ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_HOME);

                if (chromeHomeExperimentEnabled != sChromeHomeEnabled) {
                    sChromeHomeEnabled = chromeHomeExperimentEnabled;
                    ChromePreferenceManager.getInstance().setChromeHomeEnabled(
                            chromeHomeExperimentEnabled);
                }
            }
        }

        return sChromeHomeEnabled;
    }

    /**
     * Resets whether Chrome Home is enabled for tests. After this is called, the next call to
     * #isChromeHomeEnabled() will retrieve the value from shared preferences.
     */
    public static void resetChromeHomeEnabledForTests() {
        sChromeHomeEnabled = null;
    }

    /**
     * @return Whether or not the expand button for Chrome Home is enabled.
     */
    public static boolean isChromeHomeExpandButtonEnabled() {
        if (!ChromeFeatureList.isInitialized()) return false;
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_HOME_EXPAND_BUTTON);
    }

    /**
     * @return The type of swipe logic used for opening the bottom sheet in Chrome Home. Null is
     *         returned if the command line is not initialized or no experiment is specified.
     */
    public static String getChromeHomeSwipeLogicType() {
        if (sChromeHomeSwipeLogicType == null && CommandLine.isInitialized()) {
            CommandLine instance = CommandLine.getInstance();
            sChromeHomeSwipeLogicType =
                    instance.getSwitchValue(ChromeSwitches.CHROME_HOME_SWIPE_LOGIC);
        }

        return sChromeHomeSwipeLogicType;
    }

    private static native void nativeSetCustomTabVisible(boolean visible);
    private static native void nativeSetIsInMultiWindowMode(boolean isInMultiWindowMode);
}
