// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Handler;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarContainerLayout.Item;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SurveyInfoBar;
import org.chromium.chrome.browser.infobar.SurveyInfoBarDelegate;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.components.variations.VariationsAssociatedData;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Calendar;
import java.util.Random;

/**
 * Class that controls if and when to show surveys related to the Chrome Home experiment.
 */
public class ChromeHomeSurveyController implements InfoBarContainer.InfoBarAnimationListener {
    public static final String SURVEY_INFO_BAR_DISPLAYED_KEY =
            "chrome_home_survey_info_bar_displayed";
    public static final String PARAM_NAME = "survey_override_site_id";
    public static final long REQUIRED_VISIBILITY_DURATION_MS = 5000;

    private static final String TRIAL_NAME = "ChromeHome";
    private static final String MAX_NUMBER = "MaxNumber";

    static final long ONE_WEEK_IN_MILLIS = 604800000L;
    static final String DATE_LAST_ROLLED_KEY = "chrome_home_date_last_rolled_for_survey";

    private TabModelSelector mTabModelSelector;
    private Handler mLoggingHandler;
    private Tab mSurveyInfoBarTab;
    private TabModelSelectorObserver mTabModelObserver;

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
        assert tabModelSelector != null;
        assert context != null;
        new StartDownloadIfEligibleTask(new ChromeHomeSurveyController(), context, tabModelSelector)
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Downloads the survey if the user qualifies.
     * @param context The current Android {@link Context}.
     * @param tabModelSelector The tab model selector to access the tab on which the survey will be
     *                         shown.
     */
    private void startDownload(Context context, TabModelSelector tabModelSelector) {
        mLoggingHandler = new Handler();
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

    /** @return Whether the user qualifies for the survey. */
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

    /**
     * Called when the survey has finished downloading to show the survey info bar.
     * @param siteId The site id of the survey to display.
     */
    @VisibleForTesting
    void onSurveyAvailable(String siteId) {
        if (attemptToShowOnTab(mTabModelSelector.getCurrentTab(), siteId)) return;

        mTabModelObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onChange() {
                attemptToShowOnTab(mTabModelSelector.getCurrentTab(), siteId);
            }
        };

        mTabModelSelector.addObserver(mTabModelObserver);
    }

    /**
     * Show the survey info bar on the passed in tab if the tab is finished loading.
     * Else, it adds a listener to the tab to show the info bar once conditions are met.
     * @param tab The tab to attempt to attach the survey info bar.
     * @param siteId The site id of the survey to display.
     * @return If the infobar was successfully shown.
     */
    private boolean attemptToShowOnTab(Tab tab, String siteId) {
        if (!isValidTabForSurvey(tab)) return false;

        if (tab.isLoading() || !tab.isUserInteractable()) {
            tab.addObserver(createTabObserver(tab, siteId));
            return false;
        }

        showSurveyInfoBar(tab, siteId);
        return true;
    }

    /**
     * Shows the survey info bar.
     * @param tab The tab to attach the survey info bar.
     * @param siteId The site id of the survey to display.
     */
    @VisibleForTesting
    void showSurveyInfoBar(Tab tab, String siteId) {
        mSurveyInfoBarTab = tab;
        tab.getInfoBarContainer().addAnimationListener(this);
        SurveyInfoBar.showSurveyInfoBar(tab.getWebContents(), siteId, true,
                R.drawable.chrome_sync_logo, getSurveyInfoBarDelegate());

        mTabModelSelector.removeObserver(mTabModelObserver);
    }

    /**
     * @return The observer that handles cases where the user switches tabs before an infobar is
     *         shown.
     */
    private TabObserver createTabObserver(Tab tab, String siteId) {
        return new EmptyTabObserver() {
            @Override
            public void onInteractabilityChanged(boolean isInteractable) {
                showInfoBarIfApplicable(tab, siteId, this);
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                showInfoBarIfApplicable(tab, siteId, this);
            }

            @Override
            public void onHidden(Tab tab) {
                // An infobar shouldn't appear on a tab that the user left.
                tab.removeObserver(this);
            }
        };
    }

    /**
     * Shows the infobar if the passed in tab is fully loaded and interactable.
     * @param tab The tab to attach the survey info bar.
     * @param siteId The site id of the survey to display.
     * @param observer The tab observer to remove from the passed in tab.
     */
    @VisibleForTesting
    void showInfoBarIfApplicable(Tab tab, String siteId, TabObserver observer) {
        if (!tab.isUserInteractable() || tab.isLoading()) return;

        showSurveyInfoBar(tab, siteId);
        tab.removeObserver(observer);
    }

    /** @return Whether the user has consented to reporting usage metrics and crash dumps. */
    private boolean isUMAEnabled() {
        return PrivacyPreferencesManager.getInstance().isUsageAndCrashReportingPermittedByUser();
    }

    /** @return If the survey info bar was logged as seen before. */
    @VisibleForTesting
    boolean hasInfoBarBeenDisplayed() {
        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        return sharedPreferences.getLong(SURVEY_INFO_BAR_DISPLAYED_KEY, -1L) != -1L;
    }

    /** @return If it has been over a week since ChromeHome was enabled. */
    @VisibleForTesting
    boolean wasChromeHomeEnabledForMinimumOneWeek() {
        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        long earliestLoggedDate = sharedPreferences.getLong(
                ChromePreferenceManager.CHROME_HOME_SHARED_PREFERENCES_KEY, Long.MAX_VALUE);
        if (System.currentTimeMillis() - earliestLoggedDate >= ONE_WEEK_IN_MILLIS) return true;
        return false;
    }

    /**
     * Checks if the tab is valid for a survey (i.e. not null, no null webcontents & not incognito).
     * @param tab The tab to be checked.
     * @return Whether or not the tab is valid.
     */
    @VisibleForTesting
    boolean isValidTabForSurvey(Tab tab) {
        return tab != null && tab.getWebContents() != null && !tab.isIncognito();
    }

    /** @return A new {@link ChromeHomeSurveyController} for testing. */
    @VisibleForTesting
    public static ChromeHomeSurveyController createChromeHomeSurveyControllerForTests() {
        return new ChromeHomeSurveyController();
    }

    @Override
    public void notifyAnimationFinished(int animationType) {}

    @Override
    public void notifyAllAnimationsFinished(Item frontInfoBar) {
        mLoggingHandler.removeCallbacksAndMessages(null);

        // If the survey info bar is in front, start the countdown to log that it was displayed.
        if (frontInfoBar == null
                || frontInfoBar.getInfoBarIdentifier()
                        != InfoBarIdentifier.SURVEY_INFOBAR_ANDROID) {
            return;
        }

        mLoggingHandler.postDelayed(
                () -> recordInfoBarDisplayed(), REQUIRED_VISIBILITY_DURATION_MS);
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
            public void onSurveyInfoBarTabInteractabilityChanged(boolean isInteractable) {
                if (mSurveyInfoBarTab == null) return;

                if (!isInteractable) {
                    mLoggingHandler.removeCallbacksAndMessages(null);
                    return;
                }

                mLoggingHandler.postDelayed(
                        () -> recordInfoBarDisplayed(), REQUIRED_VISIBILITY_DURATION_MS);
            }

            @Override
            public void onSurveyInfoBarTabHidden() {
                mLoggingHandler.removeCallbacksAndMessages(null);
                mSurveyInfoBarTab = null;
            }

            @Override
            public void onSurveyInfoBarClosed() {
                recordInfoBarDisplayed();
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

    /** Logs in {@link SharedPreferences} that the info bar was displayed. */
    private void recordInfoBarDisplayed() {
        if (mSurveyInfoBarTab.getInfoBarContainer() != null) {
            mSurveyInfoBarTab.getInfoBarContainer().removeAnimationListener(this);
        }
        mLoggingHandler.removeCallbacksAndMessages(null);

        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        sharedPreferences.edit()
                .putLong(SURVEY_INFO_BAR_DISPLAYED_KEY, System.currentTimeMillis())
                .apply();
        mSurveyInfoBarTab = null;
    }

    @VisibleForTesting
    void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
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
