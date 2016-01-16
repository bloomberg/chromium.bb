// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.components.variations.VariationsAssociatedData;

/**
 * Provides Field Trial support for the Contextual Search application within Chrome for Android.
 */
public class ContextualSearchFieldTrial {
    private static final String FIELD_TRIAL_NAME = "ContextualSearch";
    private static final String DISABLED_PARAM = "disabled";
    private static final String ENABLED_VALUE = "true";

    private static final String PEEK_PROMO_FORCED = "peek_promo_forced";
    @VisibleForTesting
    static final String PEEK_PROMO_ENABLED = "peek_promo_enabled";
    private static final String PEEK_PROMO_MAX_SHOW_COUNT = "peek_promo_max_show_count";
    private static final int PEEK_PROMO_DEFAULT_MAX_SHOW_COUNT = 10;

    private static final String DISABLE_SEARCH_TERM_RESOLUTION = "disable_search_term_resolution";
    private static final String DISABLE_EXTRA_SEARCH_BAR_ANIMATIONS =
            "disable_extra_search_bar_animations";
    private static final String ENABLE_DIGIT_BLACKLIST = "enable_digit_blacklist";

    // Translation.  All these members are private, except for usage by testing.
    // Master switch, needed to enable any translate code for Contextual Search.
    @VisibleForTesting
    static final String ENABLE_TRANSLATION = "enable_translation";
    // Switch to disable translation, but not logging, used for experiment comparison.
    @VisibleForTesting
    static final String DISABLE_FORCE_TRANSLATION_ONEBOX = "disable_force_translation_onebox";
    // Disables translation when we need to auto-detect the source language (when we don't resolve).
    @VisibleForTesting
    static final String DISABLE_AUTO_DETECT_TRANSLATION_ONEBOX =
            "disable_auto_detect_translation_onebox";
    // Disables using the keyboard languages to determine the target language.
    private static final String DISABLE_KEYBOARD_LANGUAGES_FOR_TRANSLATION =
            "disable_keyboard_languages_for_translation";
    // Disables using the accept-languages list to determine the target language.
    private static final String DISABLE_ACCEPT_LANGUAGES_FOR_TRANSLATION =
            "disable_accept_languages_for_translation";
    // Enables usage of English as the target language even when it's the primary UI language.
    private static final String ENABLE_ENGLISH_TARGET_TRANSLATION =
            "enable_english_target_translation";
    // Enables relying on the server to control whether the onebox is actually shown, rather
    // than checking if translation is needed client-side based on source/target languages.
    @VisibleForTesting
    static final String ENABLE_SERVER_CONTROLLED_ONEBOX = "enable_server_controlled_onebox";

    // Quick Answers.
    private static final String ENABLE_QUICK_ANSWERS = "enable_quick_answers";

    // Cached values to avoid repeated and redundant JNI operations.
    private static Boolean sEnabled;
    private static Boolean sDisableSearchTermResolution;
    private static Boolean sIsPeekPromoEnabled;
    private static Integer sPeekPromoMaxCount;
    private static Boolean sIsTranslationEnabled;
    private static Boolean sIsForceTranslationOneboxDisabled;
    private static Boolean sIsAutoDetectTranslationOneboxDisabled;
    private static Boolean sIsAcceptLanguagesForTranslationDisabled;
    private static Boolean sIsKeyboardLanguagesForTranslationDisabled;
    private static Boolean sIsEnglishTargetTranslationEnabled;
    private static Boolean sIsServerControlledOneboxEnabled;
    private static Boolean sIsQuickAnswersEnabled;

    /**
     * Don't instantiate.
     */
    private ContextualSearchFieldTrial() {}

    /**
     * Checks the current Variations parameters associated with the active group as well as the
     * Chrome preference to determine if the service is enabled.
     * @param context Context used to determine whether the device is a tablet or a phone.
     * @return Whether Contextual Search is enabled or not.
     */
    public static boolean isEnabled(Context context) {
        if (sEnabled == null) {
            sEnabled = detectEnabled(context);
        }
        return sEnabled.booleanValue();
    }

    private static boolean detectEnabled(Context context) {
        if (SysUtils.isLowEndDevice()) {
            return false;
        }

        // This is used for instrumentation tests (i.e. it is not a user-flippable flag). We cannot
        // use Variations params because in the test harness, the initialization comes before any
        // native methods are available. And the ContextualSearchManager is initialized very early
        // in the Chrome initialization.
        if (CommandLine.getInstance().hasSwitch(
                    ChromeSwitches.ENABLE_CONTEXTUAL_SEARCH_FOR_TESTING)) {
            return true;
        }

        // Allow this user-flippable flag to disable the feature.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_CONTEXTUAL_SEARCH)) {
            return false;
        }

        // Allow this user-flippable flag to enable the feature.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_CONTEXTUAL_SEARCH)) {
            return true;
        }

        // Allow disabling the feature remotely.
        if (getBooleanParam(DISABLED_PARAM)) {
            return false;
        }

        return true;
    }

    /**
     * @return Whether the search term resolution is enabled.
     */
    static boolean isSearchTermResolutionEnabled() {
        if (sDisableSearchTermResolution == null) {
            sDisableSearchTermResolution = getBooleanParam(DISABLE_SEARCH_TERM_RESOLUTION);
        }

        if (sDisableSearchTermResolution.booleanValue()) {
            return false;
        }

        return true;
    }

    /**
     * @return Whether the Peek Promo is forcibly enabled (used for testing).
     */
    static boolean isPeekPromoForced() {
        return CommandLine.getInstance().hasSwitch(PEEK_PROMO_FORCED);
    }

    /**
     * @return Whether the Peek Promo is enabled.
     */
    static boolean isPeekPromoEnabled() {
        if (sIsPeekPromoEnabled == null) {
            sIsPeekPromoEnabled = getBooleanParam(PEEK_PROMO_ENABLED);
        }
        return sIsPeekPromoEnabled.booleanValue();
    }

    /**
     * @return Whether extra search bar animations are disabled.
     */
    static boolean areExtraSearchBarAnimationsDisabled() {
        return getBooleanParam(DISABLE_EXTRA_SEARCH_BAR_ANIMATIONS);
    }

    /**
     * @return Whether the digit blacklist is enabled.
     */
    static boolean isDigitBlacklistEnabled() {
        return getBooleanParam(ENABLE_DIGIT_BLACKLIST);
    }

    /**
     * @return The maximum number of times the Peek Promo should be displayed.
     */
    static int getPeekPromoMaxShowCount() {
        if (sPeekPromoMaxCount == null) {
            sPeekPromoMaxCount = getIntParamValueOrDefault(
                    PEEK_PROMO_MAX_SHOW_COUNT,
                    PEEK_PROMO_DEFAULT_MAX_SHOW_COUNT);
        }
        return sPeekPromoMaxCount.intValue();
    }

    /**
     * @return Whether any translate code is enabled.
     */
    static boolean isTranslationEnabled() {
        if (sIsTranslationEnabled == null) {
            sIsTranslationEnabled = getBooleanParam(ENABLE_TRANSLATION);
        }
        return sIsTranslationEnabled.booleanValue();
    }

    /**
     * @return Whether forcing a translation Onebox is disabled.
     */
    static boolean isForceTranslationOneboxDisabled() {
        if (sIsForceTranslationOneboxDisabled == null) {
            sIsForceTranslationOneboxDisabled = getBooleanParam(DISABLE_FORCE_TRANSLATION_ONEBOX);
        }
        return sIsForceTranslationOneboxDisabled.booleanValue();
    }

    /**
     * @return Whether forcing a translation Onebox based on auto-detection of the source language
     *         is disabled.
     */
    static boolean isAutoDetectTranslationOneboxDisabled() {
        if (sIsAutoDetectTranslationOneboxDisabled == null) {
            sIsAutoDetectTranslationOneboxDisabled = getBooleanParam(
                    DISABLE_AUTO_DETECT_TRANSLATION_ONEBOX);
        }
        return sIsAutoDetectTranslationOneboxDisabled.booleanValue();
    }

    /**
     * @return Whether considering accept-languages for translation is disabled.
     */
    static boolean isAcceptLanguagesForTranslationDisabled() {
        if (sIsAcceptLanguagesForTranslationDisabled == null) {
            sIsAcceptLanguagesForTranslationDisabled = getBooleanParam(
                    DISABLE_ACCEPT_LANGUAGES_FOR_TRANSLATION);
        }
        return sIsAcceptLanguagesForTranslationDisabled.booleanValue();
    }

    /**
     * @return Whether considering keyboards for translation is disabled.
     */
    static boolean isKeyboardLanguagesForTranslationDisabled() {
        if (sIsKeyboardLanguagesForTranslationDisabled == null) {
            sIsKeyboardLanguagesForTranslationDisabled =
                    getBooleanParam(DISABLE_KEYBOARD_LANGUAGES_FOR_TRANSLATION);
        }
        return sIsKeyboardLanguagesForTranslationDisabled.booleanValue();
    }

    /**
     * @return Whether English-target translation should be enabled (default is disabled for 'en').
     */
    static boolean isEnglishTargetTranslationEnabled() {
        if (sIsEnglishTargetTranslationEnabled == null) {
            sIsEnglishTargetTranslationEnabled = getBooleanParam(ENABLE_ENGLISH_TARGET_TRANSLATION);
        }
        return sIsEnglishTargetTranslationEnabled.booleanValue();
    }

    /**
     * @return Whether relying on server-control of showing the translation one-box is enabled.
     */
    static boolean isServerControlledOneboxEnabled() {
        if (sIsServerControlledOneboxEnabled == null) {
            sIsServerControlledOneboxEnabled = getBooleanParam(ENABLE_SERVER_CONTROLLED_ONEBOX);
        }
        return sIsServerControlledOneboxEnabled.booleanValue();
    }

    /**
     * @return Whether showing "quick answers" in the Bar is enabled.
     */
    static boolean isQuickAnswersEnabled() {
        if (sIsQuickAnswersEnabled == null) {
            sIsQuickAnswersEnabled = getBooleanParam(ENABLE_QUICK_ANSWERS);
        }
        return sIsQuickAnswersEnabled.booleanValue();
    }

    // --------------------------------------------------------------------------------------------
    // Helpers.
    // --------------------------------------------------------------------------------------------

    /**
     * Gets a boolean Finch parameter, assuming the <paramName>="true" format.  Also checks for a
     * command-line switch with the same name, for easy local testing.
     * @param paramName The name of the Finch parameter (or command-line switch) to get a value for.
     * @return Whether the Finch param is defined with a value "true", if there's a command-line
     *         flag present with any value.
     */
    private static boolean getBooleanParam(String paramName) {
        if (CommandLine.getInstance().hasSwitch(paramName)) {
            return true;
        }
        return TextUtils.equals(ENABLED_VALUE,
                VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName));
    }

    /**
     * Returns an integer value for a Finch parameter, or the default value if no parameter exists
     * in the current configuration.  Also checks for a command-line switch with the same name.
     * @param paramName The name of the Finch parameter (or command-line switch) to get a value for.
     * @param defaultValue The default value to return when there's no param or switch.
     * @return An integer value -- either the param or the default.
     */
    private static int getIntParamValueOrDefault(String paramName, int defaultValue) {
        String value = CommandLine.getInstance().getSwitchValue(paramName);
        if (TextUtils.isEmpty(value)) {
            value = VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName);
        }
        if (!TextUtils.isEmpty(value)) {
            try {
                return Integer.parseInt(value);
            } catch (NumberFormatException e) {
                return defaultValue;
            }
        }

        return defaultValue;
    }
}
