// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.SharedPreferences;
import android.os.StrictMode;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.crash.MinidumpUploadService.ProcessType;

import java.util.Locale;
import java.util.Set;

/**
 * ChromePreferenceManager stores and retrieves various values in Android shared preferences.
 */
public class ChromePreferenceManager {
    private static final String TAG = "preferences";

    private static final String PROMOS_SKIPPED_ON_FIRST_START = "promos_skipped_on_first_start";
    private static final String SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION =
            "signin_promo_last_shown_chrome_version";
    private static final String SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES =
            "signin_promo_last_shown_account_names";
    private static final String ALLOW_LOW_END_DEVICE_UI = "allow_low_end_device_ui";
    private static final String PREF_WEBSITE_SETTINGS_FILTER = "website_settings_filter";
    private static final String CARDS_IMPRESSION_AFTER_ANIMATION =
            "cards_impression_after_animation";
    private static final String CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT =
            "contextual_search_promo_open_count";
    private static final String CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT =
            "contextual_search_tap_triggered_promo_count";
    private static final String CONTEXTUAL_SEARCH_TAP_COUNT = "contextual_search_tap_count";
    private static final String CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME =
            "contextual_search_last_animation_time";
    private static final String CONTEXTUAL_SEARCH_TAP_QUICK_ANSWER_COUNT =
            "contextual_search_tap_quick_answer_count";
    private static final String CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER =
            "contextual_search_current_week_number";
    private static final String CHROME_HOME_ENABLED_KEY = "chrome_home_enabled";
    private static final String CHROME_HOME_USER_ENABLED_KEY = "chrome_home_user_enabled";
    private static final String CHROME_HOME_OPT_OUT_SNACKBAR_SHOWN =
            "chrome_home_opt_out_snackbar_shown";

    private static final String CHROME_DEFAULT_BROWSER = "applink.chrome_default_browser";

    private static final String CONTENT_SUGGESTIONS_SHOWN_KEY = "content_suggestions_shown";

    private static final String SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED =
            "settings_personalized_signin_promo_dismissed";

    private static final String NTP_SIGNIN_PROMO_DISMISSED =
            "ntp.personalized_signin_promo_dismissed";
    private static final String NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START =
            "ntp.signin_promo_suppression_period_start";
    private static final String NTP_ANIMATION_RUN_COUNT = "ntp_recycler_view_animation_run_count";

    private static final String SUCCESS_UPLOAD_SUFFIX = "_crash_success_upload";
    private static final String FAILURE_UPLOAD_SUFFIX = "_crash_failure_upload";

    public static final String CHROME_HOME_SHARED_PREFERENCES_KEY = "chrome_home_enabled_date";

    public static final String CHROME_HOME_INFO_PROMO_SHOWN_KEY = "chrome_home_info_promo_shown";

    private static final String CHROME_HOME_MENU_ITEM_CLICK_COUNT_KEY =
            "chrome_home_menu_item_click_count";
    private static final String SOLE_INTEGRATION_ENABLED_KEY = "sole_integration_enabled";

    private static class LazyHolder {
        static final ChromePreferenceManager INSTANCE = new ChromePreferenceManager();
    }

    private final SharedPreferences mSharedPreferences;

    private ChromePreferenceManager() {
        mSharedPreferences = ContextUtils.getAppSharedPreferences();
    }

    /**
     * Get the static instance of ChromePreferenceManager if exists else create it.
     * @return the ChromePreferenceManager singleton
     */
    public static ChromePreferenceManager getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * @return Number of times of successful crash upload.
     */
    public int getCrashSuccessUploadCount(@ProcessType String process) {
        // Convention to keep all the key in preference lower case.
        return mSharedPreferences.getInt(successUploadKey(process), 0);
    }

    public void setCrashSuccessUploadCount(@ProcessType String process, int count) {
        SharedPreferences.Editor sharedPreferencesEditor;

        sharedPreferencesEditor = mSharedPreferences.edit();
        // Convention to keep all the key in preference lower case.
        sharedPreferencesEditor.putInt(successUploadKey(process), count);
        sharedPreferencesEditor.apply();
    }

    public void incrementCrashSuccessUploadCount(@ProcessType String process) {
        setCrashSuccessUploadCount(process, getCrashSuccessUploadCount(process) + 1);
    }

    private String successUploadKey(@ProcessType String process) {
        return process.toLowerCase(Locale.US) + SUCCESS_UPLOAD_SUFFIX;
    }

    /**
     * @return Number of times of failure crash upload after reaching the max number of tries.
     */
    public int getCrashFailureUploadCount(@ProcessType String process) {
        return mSharedPreferences.getInt(failureUploadKey(process), 0);
    }

    public void setCrashFailureUploadCount(@ProcessType String process, int count) {
        SharedPreferences.Editor sharedPreferencesEditor;

        sharedPreferencesEditor = mSharedPreferences.edit();
        sharedPreferencesEditor.putInt(failureUploadKey(process), count);
        sharedPreferencesEditor.apply();
    }

    public void incrementCrashFailureUploadCount(@ProcessType String process) {
        setCrashFailureUploadCount(process, getCrashFailureUploadCount(process) + 1);
    }

    private String failureUploadKey(@ProcessType String process) {
        return process.toLowerCase(Locale.US) + FAILURE_UPLOAD_SUFFIX;
    }

    /**
     * @return Whether the promotion for data reduction has been skipped on first invocation.
     */
    public boolean getPromosSkippedOnFirstStart() {
        return mSharedPreferences.getBoolean(PROMOS_SKIPPED_ON_FIRST_START, false);
    }

    /**
     * Marks whether the data reduction promotion was skipped on first
     * invocation.
     * @param displayed Whether the promotion was shown.
     */
    public void setPromosSkippedOnFirstStart(boolean displayed) {
        writeBoolean(PROMOS_SKIPPED_ON_FIRST_START, displayed);
    }

    /**
     * @return The value for the website settings filter (the one that specifies
     * which sites to show in the list).
     */
    public String getWebsiteSettingsFilterPreference() {
        return mSharedPreferences.getString(
                ChromePreferenceManager.PREF_WEBSITE_SETTINGS_FILTER, "");
    }

    /**
     * Sets the filter value for website settings (which websites to show in the list).
     * @param prefValue The type to restrict the filter to.
     */
    public void setWebsiteSettingsFilterPreference(String prefValue) {
        SharedPreferences.Editor sharedPreferencesEditor = mSharedPreferences.edit();
        sharedPreferencesEditor.putString(
                ChromePreferenceManager.PREF_WEBSITE_SETTINGS_FILTER, prefValue);
        sharedPreferencesEditor.apply();
    }

    /**
     * This value may have been explicitly set to false when we used to keep existing low-end
     * devices on the normal UI rather than the simplified UI. We want to keep the existing device
     * settings. For all new low-end devices they should get the simplified UI by default.
     * @return Whether low end device UI was allowed.
     */
    public boolean getAllowLowEndDeviceUi() {
        return mSharedPreferences.getBoolean(ALLOW_LOW_END_DEVICE_UI, true);
    }

    /**
     * Returns Chrome major version number when signin promo was last shown, or 0 if version number
     * isn't known.
     */
    public int getSigninPromoLastShownVersion() {
        return mSharedPreferences.getInt(SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION, 0);
    }

    /**
     * Sets Chrome major version number when signin promo was last shown.
     */
    public void setSigninPromoLastShownVersion(int majorVersion) {
        SharedPreferences.Editor editor = mSharedPreferences.edit();
        editor.putInt(SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION, majorVersion).apply();
    }

    /**
     * Returns a set of account names on the device when signin promo was last shown,
     * or null if promo hasn't been shown yet.
     */
    public Set<String> getSigninPromoLastAccountNames() {
        return mSharedPreferences.getStringSet(SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, null);
    }

    /**
     * Stores a set of account names on the device when signin promo is shown.
     */
    public void setSigninPromoLastAccountNames(Set<String> accountNames) {
        SharedPreferences.Editor editor = mSharedPreferences.edit();
        editor.putStringSet(SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, accountNames).apply();
    }

    /**
     * @return Number of times the panel was opened with the promo visible.
     */
    public int getContextualSearchPromoOpenCount() {
        return mSharedPreferences.getInt(CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT, 0);
    }

    /**
     * Sets the number of times the panel was opened with the promo visible.
     * @param count Number of times the panel was opened with a promo visible.
     */
    public void setContextualSearchPromoOpenCount(int count) {
        writeInt(CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT, count);
    }

    /**
     * @return The last time the search provider icon was animated on tap.
     */
    public long getContextualSearchLastAnimationTime() {
        return mSharedPreferences.getLong(CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME, 0);
    }

    /**
     * Sets the last time the search provider icon was animated on tap.
     * @param time The last time the search provider icon was animated on tap.
     */
    public void setContextualSearchLastAnimationTime(long time) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putLong(CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME, time);
        ed.apply();
    }

    /**
     * @return Number of times the promo was triggered to peek by a tap gesture, or a negative value
     *         if in the disabled state.
     */
    public int getContextualSearchTapTriggeredPromoCount() {
        return mSharedPreferences.getInt(CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT, 0);
    }

    /**
     * Sets the number of times the promo was triggered to peek by a tap gesture.
     * Use a negative value to record that the counter has been disabled.
     * @param count Number of times the promo was triggered by a tap gesture, or a negative value
     *        to record that the counter has been disabled.
     */
    public void setContextualSearchTapTriggeredPromoCount(int count) {
        writeInt(CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT, count);
    }

    /**
     * @return Number of tap gestures that have been received since the last time the panel was
     *         opened.
     */
    public int getContextualSearchTapCount() {
        return mSharedPreferences.getInt(CONTEXTUAL_SEARCH_TAP_COUNT, 0);
    }

    /**
     * Sets the number of tap gestures that have been received since the last time the panel was
     * opened.
     * @param count Number of taps that have been received since the last time the panel was opened.
     */
    public void setContextualSearchTapCount(int count) {
        writeInt(CONTEXTUAL_SEARCH_TAP_COUNT, count);
    }

    /**
     * @return Number of Tap triggered Quick Answers (that "do answer") that have been shown since
     *         the last time the panel was opened.
     */
    public int getContextualSearchTapQuickAnswerCount() {
        return mSharedPreferences.getInt(CONTEXTUAL_SEARCH_TAP_QUICK_ANSWER_COUNT, 0);
    }

    /**
     * Sets the number of tap triggered Quick Answers (that "do answer") that have been shown since
     * the last time the panel was opened.
     * @param count Number of Tap triggered Quick Answers (that "do answer") that have been shown
     *              since the last time the panel was opened.
     */
    public void setContextualSearchTapQuickAnswerCount(int count) {
        writeInt(CONTEXTUAL_SEARCH_TAP_QUICK_ANSWER_COUNT, count);
    }

    /**
     * @return The current week number, persisted for weekly CTR recording.
     */
    public int getContextualSearchCurrentWeekNumber() {
        return mSharedPreferences.getInt(CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER, 0);
    }

    /**
     * Sets the current week number to persist.  Used for weekly CTR recording.
     * @param weekNumber The week number to store.
     */
    public void setContextualSearchCurrentWeekNumber(int weekNumber) {
        writeInt(CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER, weekNumber);
    }

    public boolean getCachedChromeDefaultBrowser() {
        return mSharedPreferences.getBoolean(CHROME_DEFAULT_BROWSER, false);
    }

    public void setCachedChromeDefaultBrowser(boolean isDefault) {
        writeBoolean(CHROME_DEFAULT_BROWSER, isDefault);
    }

    /** Set whether the user dismissed the personalized sign in promo from the Settings. */
    public void setSettingsPersonalizedSigninPromoDismissed(boolean isPromoDismissed) {
        writeBoolean(SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED, isPromoDismissed);
    }

    /** Checks if the user dismissed the personalized sign in promo from the Settings. */
    public boolean getSettingsPersonalizedSigninPromoDismissed() {
        return mSharedPreferences.getBoolean(SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED, false);
    }

    /** Checks if the user dismissed the personalized sign in promo from the new tab page. */
    public boolean getNewTabPageSigninPromoDismissed() {
        return mSharedPreferences.getBoolean(NTP_SIGNIN_PROMO_DISMISSED, false);
    }

    /** Set whether the user dismissed the personalized sign in promo from the new tab page. */
    public void setNewTabPageSigninPromoDismissed(boolean isPromoDismissed) {
        writeBoolean(NTP_SIGNIN_PROMO_DISMISSED, isPromoDismissed);
    }

    /**
     * Returns timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed; zero otherwise.
     * @return the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    public long getNewTabPageSigninPromoSuppressionPeriodStart() {
        return readLong(NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START, 0);
    }

    /**
     * Sets timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed.
     * @param timeMillis the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    public void setNewTabPageSigninPromoSuppressionPeriodStart(long timeMillis) {
        writeLong(NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START, timeMillis);
    }

    /**
     * Removes the stored timestamp of the suppression period start when signin promos in the New
     * Tab Page are no longer suppressed.
     */
    public void clearNewTabPageSigninPromoSuppressionPeriodStart() {
        removeKey(NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START);
    }

    /** Gets the number of times the New Tab Page first card animation has been run. */
    public int getNewTabPageFirstCardAnimationRunCount() {
        return readInt(NTP_ANIMATION_RUN_COUNT);
    }

    /** Records the number of times the New Tab Page first card animation has been run. */
    public void setNewTabPageFirstCardAnimationRunCount(int value) {
        writeInt(NTP_ANIMATION_RUN_COUNT, value);
    }

    /** Returns whether the user has triggered a snippet impression after viewing the animation. */
    public boolean getCardsImpressionAfterAnimation() {
        return mSharedPreferences.getBoolean(CARDS_IMPRESSION_AFTER_ANIMATION, false);
    }

    /** Sets whether the user has triggered a snippet impression after viewing the animation. */
    public void setCardsImpressionAfterAnimation(boolean isScrolled) {
        writeBoolean(CARDS_IMPRESSION_AFTER_ANIMATION, isScrolled);
    }

    /**
     * Set whether or not Chrome Home is enabled.
     * @param isEnabled If Chrome Home is enabled.
     */
    public void setChromeHomeEnabled(boolean isEnabled) {
        writeBoolean(CHROME_HOME_ENABLED_KEY, isEnabled);
    }

    /**
     * Get whether or not Chrome Home is enabled.
     * @return True if Chrome Home is enabled.
     */
    public boolean isChromeHomeEnabled() {
        return mSharedPreferences.getBoolean(CHROME_HOME_ENABLED_KEY, false);
    }

    /**
     * Set whether or not Chrome Home is enabled by the user.
     * @param isEnabled If Chrome Home is enabled by the user.
     */
    public void setChromeHomeUserEnabled(boolean isEnabled) {
        writeBoolean(CHROME_HOME_USER_ENABLED_KEY, isEnabled);
    }

    /**
     * Get whether or not Chrome Home is enabled by the user.
     * @return True if Chrome Home is enabled by the user.
     */
    public boolean isChromeHomeUserEnabled() {
        return mSharedPreferences.getBoolean(CHROME_HOME_USER_ENABLED_KEY, false);
    }

    /**
     * @return Whether or not the user has set their Chrome Home preference.
     */
    public boolean isChromeHomeUserPreferenceSet() {
        return mSharedPreferences.contains(CHROME_HOME_USER_ENABLED_KEY);
    }

    /**
     * Remove the Chrome Home user preference.
     */
    public void clearChromeHomeUserPreference() {
        removeKey(CHROME_HOME_USER_ENABLED_KEY);
    }

    /**
     * Set that the Chrome Home info-promo has been shown.
     */
    public void setChromeHomeInfoPromoShown() {
        writeBoolean(CHROME_HOME_INFO_PROMO_SHOWN_KEY, true);
    }

    /**
     * @return Whether the info-only version of the Chrome Home promo has been shown.
     */
    public boolean hasChromeHomeInfoPromoShown() {
        return mSharedPreferences.getBoolean(CHROME_HOME_INFO_PROMO_SHOWN_KEY, false);
    }

    /**
     * Mark that the Chrome Home opt-out snackbar has been shown.
     */
    public void setChromeHomeOptOutSnackbarShown() {
        writeBoolean(CHROME_HOME_OPT_OUT_SNACKBAR_SHOWN, true);
    }

    /**
     * @return Whether the Chrome Home opt-out snackbar has been shown.
     */
    public boolean getChromeHomeOptOutSnackbarShown() {
        return mSharedPreferences.getBoolean(CHROME_HOME_OPT_OUT_SNACKBAR_SHOWN, false);
    }

    /**
     * @return The number of times that bookmarks, history, or downloads have been triggered from
     *         the overflow menu while Chrome Home is enabled.
     */
    public int getChromeHomeMenuItemClickCount() {
        return readInt(CHROME_HOME_MENU_ITEM_CLICK_COUNT_KEY);
    }

    /**
     * Increment the count for the number of times bookmarks, history, or downloads have been
     * triggered from the overflow menu while Chrome Home is enabled.
     */
    public void incrementChromeHomeMenuItemClickCount() {
        writeInt(CHROME_HOME_MENU_ITEM_CLICK_COUNT_KEY, getChromeHomeMenuItemClickCount() + 1);
    }

    /**
     * Remove the count for number of times bookmarks, history, or downloads were clicked while
     * Chrome Home is enabled.
     */
    public void clearChromeHomeMenuItemClickCount() {
        mSharedPreferences.edit().remove(CHROME_HOME_MENU_ITEM_CLICK_COUNT_KEY).apply();
    }

    /** Marks that the content suggestions surface has been shown. */
    public void setSuggestionsSurfaceShown() {
        writeBoolean(CONTENT_SUGGESTIONS_SHOWN_KEY, true);
    }

    /** Returns whether the content suggestions surface has ever been shown. */
    public boolean getSuggestionsSurfaceShown() {
        return mSharedPreferences.getBoolean(CONTENT_SUGGESTIONS_SHOWN_KEY, false);
    }

    /**
     * Get whether or not Sole integration is enabled.
     * @return True if Sole integration is enabled.
     */
    public boolean isSoleEnabled() {
        return mSharedPreferences.getBoolean(SOLE_INTEGRATION_ENABLED_KEY, false);
    }

    /**
     * Set whether or not Sole integration is enabled.
     * @param isEnabled If Sole integration is enabled.
     */
    public void setSoleEnabled(boolean isEnabled) {
        writeBoolean(SOLE_INTEGRATION_ENABLED_KEY, isEnabled);
    }

    /**
     * Writes the given int value to the named shared preference.
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeInt(String key, int value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putInt(key, value);
        ed.apply();
    }

    /**
     * Reads the given int value from the named shared preference.
     * @param key The name of the preference to return.
     * @return The value of the preference.
     */
    public int readInt(String key) {
        return mSharedPreferences.getInt(key, 0);
    }

    /**
     * Writes the given long to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    private void writeLong(String key, long value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putLong(key, value);
        ed.apply();
    }

    /**
     * Reads the given long value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    private long readLong(String key, long defaultValue) {
        return mSharedPreferences.getLong(key, defaultValue);
    }

    /**
     * Writes the given String to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    private void writeString(String key, String value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putString(key, value);
        ed.apply();
    }

    /**
     * Writes the given boolean to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    private void writeBoolean(String key, boolean value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putBoolean(key, value);
        ed.apply();
    }

    /**
     * Removes the shared preference entry.
     *
     * @param key The key of the preference to remove.
     */
    private void removeKey(String key) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.remove(key);
        ed.apply();
    }

    /**
     * Logs the most recent date that Chrome Home was enabled.
     * Removes the entry if Chrome Home is disabled.
     *
     * @param isChromeHomeEnabled Whether or not Chrome Home is currently enabled.
     */
    public static void setChromeHomeEnabledDate(boolean isChromeHomeEnabled) {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
            long earliestLoggedDate =
                    sharedPreferences.getLong(CHROME_HOME_SHARED_PREFERENCES_KEY, 0L);
            if (isChromeHomeEnabled && earliestLoggedDate == 0L) {
                sharedPreferences.edit()
                        .putLong(CHROME_HOME_SHARED_PREFERENCES_KEY, System.currentTimeMillis())
                        .apply();
            } else if (!isChromeHomeEnabled && earliestLoggedDate != 0L) {
                sharedPreferences.edit().remove(CHROME_HOME_SHARED_PREFERENCES_KEY).apply();
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }
}
