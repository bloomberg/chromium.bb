// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.Resources;

import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;

/**
 * Provides Field Trial support for Omnibox Placeholder Experiment.
 */
public class OmniboxPlaceholderFieldTrial {
    private static final String FIELD_TRIAL_NAME = "OmniboxPlaceholderExperiment";
    private static final String GROUP_SEARCH_OR_TYPE_URL = "SearchOrTypeUrl";
    private static final String GROUP_SEARCH_OR_TYPE_WEBSITE_NAME = "SearchOrTypeWebsiteName";
    private static final String GROUP_SEARCH_THE_WEB = "SearchTheWeb";
    private static final String GROUP_ENTER_A_SEARCH_OR_WEBSITE = "EnterASearchOrWebsite";
    private static final String GROUP_SEARCH_NEWS = "SearchNews";
    private static final String GROUP_SEARCH_RECIPES = "SearchRecipes";
    private static final String GROUP_SEARCH_WEATHER = "SearchWeather";
    private static final String GROUP_BLANK = "Blank";

    private static String sCachedHint;

    /** Prevent initialization of this class. */
    private OmniboxPlaceholderFieldTrial() {}

    /**
     * Caches omnibox placeholder experiment group to shared preferences.
     */
    public static void cacheOmniboxPlaceholderGroup() {
        String groupName = FieldTrialList.findFullName(FIELD_TRIAL_NAME);
        ChromePreferenceManager.getInstance().setOmniboxPlaceholderGroup(groupName);
    }

    /**
     * Initialize ominibox placeholder text to static variable sCachedHint
     */
    private static void initOmniboxPlaceholder() {
        // setUrlBarHint is only called once when hint is not cached to static variable sCachedHint.
        // This is to keep consistency on showing same hint to user in one session.
        String groupName = ChromePreferenceManager.getInstance().getOmniboxPlaceholderGroup();
        Resources resources = ContextUtils.getApplicationContext().getResources();
        switch (groupName) {
            case GROUP_SEARCH_OR_TYPE_URL:
                sCachedHint = resources.getString(R.string.search_or_type_url);
                break;
            case GROUP_SEARCH_OR_TYPE_WEBSITE_NAME:
                sCachedHint = resources.getString(R.string.search_or_type_website_name);
                break;
            case GROUP_SEARCH_THE_WEB:
                sCachedHint = resources.getString(R.string.search_the_web);
                break;
            case GROUP_ENTER_A_SEARCH_OR_WEBSITE:
                sCachedHint = resources.getString(R.string.enter_a_search_or_website);
                break;
            case GROUP_SEARCH_NEWS:
                sCachedHint = resources.getString(R.string.search_news);
                break;
            case GROUP_SEARCH_RECIPES:
                sCachedHint = resources.getString(R.string.search_recipes);
                break;
            case GROUP_SEARCH_WEATHER:
                sCachedHint = resources.getString(R.string.search_weather);
                break;
            case GROUP_BLANK:
                sCachedHint = "";
                break;
            default:
                sCachedHint = resources.getString(R.string.search_or_type_url);
        }
    }

    /**
     * @return String of hint text to show in omnibox url bar.
     */
    public static String getOmniboxPlaceholder() {
        if (sCachedHint == null) initOmniboxPlaceholder();
        return sCachedHint;
    }
}
