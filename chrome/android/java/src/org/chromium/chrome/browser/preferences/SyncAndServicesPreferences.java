// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceGroup;
import android.support.annotation.Nullable;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.ListView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Settings fragment to customize Sync options (data types, encryption) and other services that
 * communicate with Google.
 */
public class SyncAndServicesPreferences
        extends PreferenceFragment implements Preference.OnPreferenceChangeListener {
    private static final String USE_SYNC_AND_ALL_SERVICES = "use_sync_and_all_services";
    private static final String SYNC_AND_PERSONALIZATION = "sync_and_personalization";
    private static final String NONPERSONALIZED_SERVICES = "nonpersonalized_services";
    private static final String PREF_NAVIGATION_ERROR = "navigation_error";
    private static final String PREF_SEARCH_SUGGESTIONS = "search_suggestions";
    private static final String PREF_SAFE_BROWSING_EXTENDED_REPORTING =
            "safe_browsing_extended_reporting";
    private static final String PREF_SAFE_BROWSING_SCOUT_REPORTING =
            "safe_browsing_scout_reporting";
    private static final String PREF_SAFE_BROWSING = "safe_browsing";
    private static final String PREF_CONTEXTUAL_SEARCH = "contextual_search";
    private static final String PREF_NETWORK_PREDICTIONS = "network_predictions";
    private static final String PREF_USAGE_AND_CRASH_REPORTING = "usage_and_crash_reports";

    private final PrefServiceBridge mPrefServiceBridge = PrefServiceBridge.getInstance();
    private final PrivacyPreferencesManager mPrivacyPrefManager =
            PrivacyPreferencesManager.getInstance();
    private final ManagedPreferenceDelegate mManagedPreferenceDelegate =
            createManagedPreferenceDelegate();

    private ChromeSwitchPreference mUseSyncAndAllServices;
    private SigninExpandablePreferenceGroup mSyncGroup;

    private SigninExpandablePreferenceGroup mNonpersonalizedServices;
    private ChromeBaseCheckBoxPreference mNavigationError;
    private ChromeBaseCheckBoxPreference mNetworkPredictions;
    private ChromeBaseCheckBoxPreference mSearchSuggestions;
    private Preference mContextualSearch;
    private ChromeBaseCheckBoxPreference mSafeBrowsingReporting;
    private ChromeBaseCheckBoxPreference mSafeBrowsing;
    private Preference mUsageAndCrashReporting;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mPrivacyPrefManager.migrateNetworkPredictionPreferences();

        getActivity().setTitle(R.string.prefs_sync_and_services);
        setHasOptionsMenu(true);

        PreferenceUtils.addPreferencesFromResource(this, R.xml.sync_and_services_preferences);

        mUseSyncAndAllServices = (ChromeSwitchPreference) findPreference(USE_SYNC_AND_ALL_SERVICES);
        mSyncGroup = (SigninExpandablePreferenceGroup) findPreference(SYNC_AND_PERSONALIZATION);
        mNonpersonalizedServices =
                (SigninExpandablePreferenceGroup) findPreference(NONPERSONALIZED_SERVICES);
        mUseSyncAndAllServices.setOnPreferenceClickListener(preference -> {
            if (!mUseSyncAndAllServices.isChecked()) {
                mSyncGroup.setExpanded(true);
                mNonpersonalizedServices.setExpanded(true);
            }
            return false;
        });

        mNetworkPredictions =
                (ChromeBaseCheckBoxPreference) findPreference(PREF_NETWORK_PREDICTIONS);
        mNetworkPredictions.setOnPreferenceChangeListener(this);
        mNetworkPredictions.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mNavigationError = (ChromeBaseCheckBoxPreference) findPreference(PREF_NAVIGATION_ERROR);
        mNavigationError.setOnPreferenceChangeListener(this);
        mNavigationError.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mSearchSuggestions = (ChromeBaseCheckBoxPreference) findPreference(PREF_SEARCH_SUGGESTIONS);
        mSearchSuggestions.setOnPreferenceChangeListener(this);
        mSearchSuggestions.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mContextualSearch = findPreference(PREF_CONTEXTUAL_SEARCH);
        if (!ContextualSearchFieldTrial.isEnabled()) {
            removePreference(mNonpersonalizedServices, mContextualSearch);
            mContextualSearch = null;
        }

        Preference extendedReporting = findPreference(PREF_SAFE_BROWSING_EXTENDED_REPORTING);
        Preference scoutReporting = findPreference(PREF_SAFE_BROWSING_SCOUT_REPORTING);
        // Remove the extended reporting preference that is NOT active.
        if (mPrefServiceBridge.isSafeBrowsingScoutReportingActive()) {
            removePreference(mNonpersonalizedServices, extendedReporting);
            mSafeBrowsingReporting = (ChromeBaseCheckBoxPreference) scoutReporting;
        } else {
            removePreference(mNonpersonalizedServices, scoutReporting);
            mSafeBrowsingReporting = (ChromeBaseCheckBoxPreference) extendedReporting;
        }
        mSafeBrowsingReporting.setOnPreferenceChangeListener(this);
        mSafeBrowsingReporting.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mSafeBrowsing = (ChromeBaseCheckBoxPreference) findPreference(PREF_SAFE_BROWSING);
        mSafeBrowsing.setOnPreferenceChangeListener(this);
        mSafeBrowsing.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mUsageAndCrashReporting = findPreference(PREF_USAGE_AND_CRASH_REPORTING);

        updatePreferences();
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        ListView list = (ListView) getView().findViewById(android.R.id.list);
        list.setDivider(null);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(R.drawable.ic_help_and_feedback);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedback.getInstance(getActivity())
                    .show(getActivity(), getString(R.string.help_context_privacy),
                            Profile.getLastUsedProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
            PrefServiceBridge.getInstance().setSearchSuggestEnabled((boolean) newValue);
        } else if (PREF_SAFE_BROWSING.equals(key)) {
            PrefServiceBridge.getInstance().setSafeBrowsingEnabled((boolean) newValue);
        } else if (PREF_SAFE_BROWSING_EXTENDED_REPORTING.equals(key)
                || PREF_SAFE_BROWSING_SCOUT_REPORTING.equals(key)) {
            PrefServiceBridge.getInstance().setSafeBrowsingExtendedReportingEnabled(
                    (boolean) newValue);
        } else if (PREF_NETWORK_PREDICTIONS.equals(key)) {
            PrefServiceBridge.getInstance().setNetworkPredictionEnabled((boolean) newValue);
            recordNetworkPredictionEnablingUMA((boolean) newValue);
        } else if (PREF_NAVIGATION_ERROR.equals(key)) {
            PrefServiceBridge.getInstance().setResolveNavigationErrorEnabled((boolean) newValue);
        }

        return true;
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    private void recordNetworkPredictionEnablingUMA(boolean enabled) {
        // Report user turning on and off NetworkPrediction.
        RecordHistogram.recordBooleanHistogram("PrefService.NetworkPredictionEnabled", enabled);
    }

    private static void removePreference(PreferenceGroup from, Preference preference) {
        boolean found = from.removePreference(preference);
        assert found : "Don't have such preference! Preference key: " + preference.getKey();
    }

    private void updatePreferences() {
        mNavigationError.setChecked(mPrefServiceBridge.isResolveNavigationErrorEnabled());
        mNetworkPredictions.setChecked(mPrefServiceBridge.getNetworkPredictionEnabled());
        mSearchSuggestions.setChecked(mPrefServiceBridge.isSearchSuggestEnabled());
        mSafeBrowsing.setChecked(mPrefServiceBridge.isSafeBrowsingEnabled());
        mSafeBrowsingReporting.setChecked(
                mPrefServiceBridge.isSafeBrowsingExtendedReportingEnabled());

        CharSequence textOn = getActivity().getResources().getText(R.string.text_on);
        CharSequence textOff = getActivity().getResources().getText(R.string.text_off);
        if (mContextualSearch != null) {
            boolean isContextualSearchEnabled = !mPrefServiceBridge.isContextualSearchDisabled();
            mContextualSearch.setSummary(isContextualSearchEnabled ? textOn : textOff);
        }
        mUsageAndCrashReporting.setSummary(
                mPrivacyPrefManager.isUsageAndCrashReportingPermittedByUser() ? textOn : textOff);
    }

    private ManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            String key = preference.getKey();
            if (PREF_NAVIGATION_ERROR.equals(key)) {
                return mPrefServiceBridge.isResolveNavigationErrorManaged();
            }
            if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
                return mPrefServiceBridge.isSearchSuggestManaged();
            }
            if (PREF_SAFE_BROWSING_EXTENDED_REPORTING.equals(key)
                    || PREF_SAFE_BROWSING_SCOUT_REPORTING.equals(key)) {
                return mPrefServiceBridge.isSafeBrowsingExtendedReportingManaged();
            }
            if (PREF_SAFE_BROWSING.equals(key)) {
                return mPrefServiceBridge.isSafeBrowsingManaged();
            }
            if (PREF_NETWORK_PREDICTIONS.equals(key)) {
                return mPrefServiceBridge.isNetworkPredictionManaged();
            }
            return false;
        };
    }
}
