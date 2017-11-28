// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.SurveyInfoBar;
import org.chromium.chrome.browser.infobar.SurveyInfoBarDelegate;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.components.variations.VariationsAssociatedData;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Calendar;
import java.util.Random;

/**
 * Class that controls if and when to show surveys related to the Chrome Home experiment.
 */
public class ChromeHomeSurveyController {
    public static final String SURVEY_INFO_BAR_DISPLAYED_KEY =
            "chrome_home_survey_info_bar_displayed";
    public static final String PARAM_NAME = "survey_override_site_id";

    private static final String TRIAL_NAME = "ChromeHome";
    private static final String MAX_NUMBER = "MaxNumber";

    static final long ONE_WEEK_IN_MILLIS = 604800000L;
    static final String SURVEY_INFOBAR_DISMISSED_KEY = "chrome_home_survey_info_bar_dismissed";
    static final String DATE_LAST_ROLLED_KEY = "chrome_home_date_last_rolled_for_survey";

    private TabModelSelector mTabModelSelector;

    @VisibleForTesting
    ChromeHomeSurveyController() {
        // Empty constructor.
    }

    /**
     * Checks if the conditions to show the survey are met and starts the process if they are.
     * @param context The current Android {@link Context}.
     * @param tabModelSelector The tab model selector to access the tab on which the survey will be
     *                         shown.
     */
    public static void initialize(Context context, TabModelSelector tabModelSelector) {
        new StartDownloadIfEligibleTask(new ChromeHomeSurveyController(), context, tabModelSelector)
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private void startDownload(Context context, TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;

        SurveyController surveyController = SurveyController.getInstance();
        CommandLine commandLine = CommandLine.getInstance();
        String siteId;
        if (commandLine.hasSwitch(PARAM_NAME)) {
            siteId = commandLine.getSwitchValue(PARAM_NAME);
        } else {
            siteId = VariationsAssociatedData.getVariationParamValue(TRIAL_NAME, PARAM_NAME);
        }

        if (TextUtils.isEmpty(siteId)) return;

        Runnable onSuccessRunnable = new Runnable() {
            @Override
            public void run() {
                onSurveyAvailable(siteId);
            }
        };
        surveyController.downloadSurvey(context, siteId, onSuccessRunnable);
    }

    private boolean doesUserQualifyForSurvey() {
        if (DeviceFormFactor.isTablet()) return false;
        if (!isUMAEnabled()) return false;
        if (AccessibilityUtil.isAccessibilityEnabled()) return false;
        if (hasInfoBarBeenDisplayed()) return false;
        if (!FeatureUtilities.isChromeHomeEnabled()) return true;
        return wasChromeHomeEnabledForMinimumOneWeek()
                || CommandLine.getInstance().hasSwitch(
                           ChromeSwitches.CHROME_HOME_FORCE_ENABLE_SURVEY);
    }

    private void onSurveyAvailable(String siteId) {
        Tab tab = mTabModelSelector.getCurrentTab();
        if (isValidTabForSurvey(tab)) {
            showSurveyInfoBar(tab, siteId);
            return;
        }

        TabModel normalModel = mTabModelSelector.getModel(false);
        normalModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, TabSelectionType type, int lastId) {
                if (isValidTabForSurvey(tab)) {
                    showSurveyInfoBar(tab, siteId);
                    normalModel.removeObserver(this);
                }
            }
        });
    }

    private void showSurveyInfoBar(Tab tab, String siteId) {
        WebContents webContents = tab.getWebContents();
        if (webContents.isLoading()) {
            webContents.addObserver(new WebContentsObserver() {
                @Override
                public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
                    if (!isMainFrame) return;
                    showSurveyInfoBar(webContents, siteId);
                    webContents.removeObserver(this);
                }
            });
        } else {
            showSurveyInfoBar(webContents, siteId);
        }
    }

    private boolean isUMAEnabled() {
        return PrivacyPreferencesManager.getInstance().isUsageAndCrashReportingPermittedByUser();
    }

    private void showSurveyInfoBar(WebContents webContents, String siteId) {
        SurveyInfoBar.showSurveyInfoBar(
                webContents, siteId, true, R.drawable.chrome_sync_logo, getSurveyInfoBarDelegate());
        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        sharedPreferences.edit()
                .putLong(SURVEY_INFO_BAR_DISPLAYED_KEY, System.currentTimeMillis())
                .apply();
    }

    @VisibleForTesting
    boolean hasInfoBarBeenDisplayed() {
        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        return sharedPreferences.getLong(SURVEY_INFO_BAR_DISPLAYED_KEY, -1L) != -1L;
    }

    @VisibleForTesting
    boolean wasChromeHomeEnabledForMinimumOneWeek() {
        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        long earliestLoggedDate = sharedPreferences.getLong(
                ChromePreferenceManager.CHROME_HOME_SHARED_PREFERENCES_KEY, Long.MAX_VALUE);
        if (System.currentTimeMillis() - earliestLoggedDate >= ONE_WEEK_IN_MILLIS) return true;
        return false;
    }

    @VisibleForTesting
    boolean isValidTabForSurvey(Tab tab) {
        return tab != null && tab.getWebContents() != null && !tab.isIncognito();
    }

    @VisibleForTesting
    public static ChromeHomeSurveyController createChromeHomeSurveyControllerForTests() {
        return new ChromeHomeSurveyController();
    }

    /**
     * Rolls a random number to see if the user was eligible for the survey
     * @return Whether the user is eligible (i.e. the random number rolled was 0).
     */
    @VisibleForTesting
    boolean isRandomlySelectedForSurvey() {
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        int lastDate = preferences.getInt(DATE_LAST_ROLLED_KEY, -1);
        int today = getDayOfYear();
        if (lastDate == today) return false;

        int maxNumber = getMaxNumber();
        if (maxNumber == -1) return false;

        preferences.edit().putInt(DATE_LAST_ROLLED_KEY, today).apply();
        return getRandomNumberUpTo(maxNumber) == 0;
    }

    /**
     * @param max The max threshold for the random number generator.
     * @return A random number from 0 (inclusive) to the max number (exclusive).
     */
    @VisibleForTesting
    int getRandomNumberUpTo(int max) {
        return new Random().nextInt(max);
    }

    /** @return The max number as stated in the finch config. */
    @VisibleForTesting
    int getMaxNumber() {
        try {
            String number = VariationsAssociatedData.getVariationParamValue(TRIAL_NAME, MAX_NUMBER);
            if (TextUtils.isEmpty(number)) return -1;
            return Integer.parseInt(number);
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    /** @return The day of the year for today. */
    @VisibleForTesting
    int getDayOfYear() {
        ThreadUtils.assertOnBackgroundThread();
        return Calendar.getInstance().get(Calendar.DAY_OF_YEAR);
    }

    /**
     * @return The survey info bar delegate containing actions specific to the Chrome Home survey.
     */
    private SurveyInfoBarDelegate getSurveyInfoBarDelegate() {
        return new SurveyInfoBarDelegate() {

            @Override
            public void onSurveyInfoBarClosed() {
                SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
                sharedPreferences.edit().putBoolean(SURVEY_INFOBAR_DISMISSED_KEY, true).apply();
            }

            @Override
            public void onSurveyTriggered() {
                RecordUserAction.record("Android.ChromeHome.AcceptedSurvey");
            }

            @Override
            public String getSurveyPromptString() {
                return ContextUtils.getApplicationContext().getString(
                        R.string.chrome_home_survey_prompt);
            }
        };
    }

    static class StartDownloadIfEligibleTask extends AsyncTask<Void, Void, Boolean> {
        final ChromeHomeSurveyController mController;
        final Context mContext;
        final TabModelSelector mSelector;

        public StartDownloadIfEligibleTask(ChromeHomeSurveyController controller, Context context,
                TabModelSelector tabModelSelector) {
            mController = controller;
            mContext = context;
            mSelector = tabModelSelector;
        }

        @Override
        protected Boolean doInBackground(Void... params) {
            if (!mController.doesUserQualifyForSurvey()) return false;
            return mController.isRandomlySelectedForSurvey()
                    || CommandLine.getInstance().hasSwitch(
                               ChromeSwitches.CHROME_HOME_FORCE_ENABLE_SURVEY);
        }

        @Override
        protected void onPostExecute(Boolean result) {
            if (result) mController.startDownload(mContext, mSelector);
        }
    }
}
