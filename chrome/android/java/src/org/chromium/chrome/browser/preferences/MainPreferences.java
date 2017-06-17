// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.preference.Preference;
import android.preference.PreferenceFragment;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.PasswordUIView;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPreferences;
import org.chromium.chrome.browser.preferences.password.SavePasswordsPreferences;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.LoadListener;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrl;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.sync.AndroidSyncSettings;

/**
 * The main settings screen, shown when the user first opens Settings.
 */
public class MainPreferences extends PreferenceFragment
        implements SignInStateObserver, Preference.OnPreferenceClickListener, LoadListener {
    public static final String PREF_SIGN_IN = "sign_in";
    public static final String PREF_DOCUMENT_MODE = "document_mode";
    public static final String PREF_AUTOFILL_SETTINGS = "autofill_settings";
    public static final String PREF_SEARCH_ENGINE = "search_engine";
    public static final String PREF_SAVED_PASSWORDS = "saved_passwords";
    public static final String PREF_HOMEPAGE = "homepage";
    public static final String PREF_SUGGESTIONS = "suggestions";
    public static final String PREF_DATA_REDUCTION = "data_reduction";

    public static final String ACCOUNT_PICKER_DIALOG_TAG = "account_picker_dialog_tag";
    public static final String EXTRA_SHOW_SEARCH_ENGINE_PICKER = "show_search_engine_picker";

    public static final String PREF_MANAGE_ACCOUNT_LINK = "manage_account_link";

    @VisibleForTesting
    public static final String VIEW_PASSWORDS = "view-passwords";

    private SignInPreference mSignInPreference;
    private ManagedPreferenceDelegate mManagedPreferenceDelegate;

    public MainPreferences() {
        setHasOptionsMenu(true);
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onResume() {
        super.onResume();
        // updatePreferences() must be called before setupSignInPref as updatePreferences loads
        // the SignInPreference.
        updatePreferences();

        if (SigninManager.get(getActivity()).isSigninSupported()) {
            SigninManager.get(getActivity()).addSignInStateObserver(this);
            setupSignInPref();
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        if (SigninManager.get(getActivity()).isSigninSupported()) {
            SigninManager.get(getActivity()).removeSignInStateObserver(this);
            clearSignInPref();
        }
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        Intent intent = new Intent(
                  Intent.ACTION_VIEW,
                  Uri.parse(PasswordUIView.getAccountDashboardURL()));
        intent.setPackage(getActivity().getPackageName());
        getActivity().startActivity(intent);
        return true;
    }

    private void updatePreferences() {
        if (getPreferenceScreen() != null) getPreferenceScreen().removeAll();

        PreferenceUtils.addPreferencesFromResource(this, R.xml.main_preferences);

        if (TemplateUrlService.getInstance().isLoaded()) {
            updateSummary();
        } else {
            TemplateUrlService.getInstance().registerLoadListener(this);
            TemplateUrlService.getInstance().load();
            ChromeBasePreference searchEnginePref =
                    (ChromeBasePreference) findPreference(PREF_SEARCH_ENGINE);
            searchEnginePref.setEnabled(false);
        }

        ChromeBasePreference autofillPref =
                (ChromeBasePreference) findPreference(PREF_AUTOFILL_SETTINGS);
        autofillPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        ChromeBasePreference passwordsPref =
                (ChromeBasePreference) findPreference(PREF_SAVED_PASSWORDS);

        ProfileSyncService syncService = ProfileSyncService.get();

        if (AndroidSyncSettings.isSyncEnabled(getActivity().getApplicationContext())
                && syncService.isEngineInitialized() && !syncService.isUsingSecondaryPassphrase()
                && ChromeFeatureList.isEnabled(VIEW_PASSWORDS)) {
            passwordsPref.setKey(PREF_MANAGE_ACCOUNT_LINK);
            passwordsPref.setTitle(R.string.redirect_to_passwords_text);
            passwordsPref.setSummary(R.string.redirect_to_passwords_link);
            passwordsPref.setOnPreferenceClickListener(this);
            passwordsPref.setManagedPreferenceDelegate(null);
        } else {
            passwordsPref.setTitle(getResources().getString(R.string.prefs_saved_passwords));
            passwordsPref.setFragment(SavePasswordsPreferences.class.getCanonicalName());
            setOnOffSummary(passwordsPref,
                    PrefServiceBridge.getInstance().isRememberPasswordsEnabled());
            passwordsPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
        }

        Preference homepagePref = findPreference(PREF_HOMEPAGE);
        if (HomepageManager.shouldShowHomepageSetting()) {
            setOnOffSummary(homepagePref,
                    HomepageManager.getInstance(getActivity()).getPrefHomepageEnabled());
        } else {
            getPreferenceScreen().removePreference(homepagePref);
        }

        ChromeBasePreference dataReduction =
                (ChromeBasePreference) findPreference(PREF_DATA_REDUCTION);
        dataReduction.setSummary(DataReductionPreferences.generateSummary(getResources()));
        dataReduction.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        if (!SigninManager.get(getActivity()).isSigninSupported()) {
            getPreferenceScreen().removePreference(findPreference(PREF_SIGN_IN));
        }
    }

    @Override
    public void onTemplateUrlServiceLoaded() {
        TemplateUrlService.getInstance().unregisterLoadListener(this);
        updateSummary();
    }

    private void updateSummary() {
        ChromeBasePreference searchEnginePref =
                (ChromeBasePreference) findPreference(PREF_SEARCH_ENGINE);
        searchEnginePref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
        searchEnginePref.setEnabled(true);

        String defaultSearchEngineName = null;
        TemplateUrl dseTemplateUrl =
                TemplateUrlService.getInstance().getDefaultSearchEngineTemplateUrl();
        if (dseTemplateUrl != null) defaultSearchEngineName = dseTemplateUrl.getShortName();
        searchEnginePref.setSummary(defaultSearchEngineName);
    }

    private void setOnOffSummary(Preference pref, boolean isOn) {
        pref.setSummary(getResources().getString(isOn ? R.string.text_on : R.string.text_off));
    }

    private void setupSignInPref() {
        mSignInPreference = (SignInPreference) findPreference(PREF_SIGN_IN);
        mSignInPreference.registerForUpdates();
    }

    private void clearSignInPref() {
        if (mSignInPreference != null) {
            mSignInPreference.unregisterForUpdates();
            mSignInPreference = null;
        }
    }

    // SignInStateObserver

    @Override
    public void onSignedIn() {
        // After signing in or out of a managed account, preferences may change or become enabled
        // or disabled.
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                updatePreferences();
            }
        });
    }

    @Override
    public void onSignedOut() {
        updatePreferences();
    }

    private ManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (PREF_AUTOFILL_SETTINGS.equals(preference.getKey())) {
                    return PersonalDataManager.isAutofillManaged();
                }
                if (PREF_SAVED_PASSWORDS.equals(preference.getKey())) {
                    return PrefServiceBridge.getInstance().isRememberPasswordsManaged();
                }
                if (PREF_DATA_REDUCTION.equals(preference.getKey())) {
                    return DataReductionProxySettings.getInstance().isDataReductionProxyManaged();
                }
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlService.getInstance().isDefaultSearchManaged();
                }
                return false;
            }

            @Override
            public boolean isPreferenceClickDisabledByPolicy(Preference preference) {
                if (PREF_AUTOFILL_SETTINGS.equals(preference.getKey())) {
                    return PersonalDataManager.isAutofillManaged()
                            && !PersonalDataManager.isAutofillEnabled();
                }
                if (PREF_SAVED_PASSWORDS.equals(preference.getKey())) {
                    PrefServiceBridge prefs = PrefServiceBridge.getInstance();
                    return prefs.isRememberPasswordsManaged()
                            && !prefs.isRememberPasswordsEnabled();
                }
                if (PREF_DATA_REDUCTION.equals(preference.getKey())) {
                    DataReductionProxySettings settings = DataReductionProxySettings.getInstance();
                    return settings.isDataReductionProxyManaged()
                            && !settings.isDataReductionProxyEnabled();
                }
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlService.getInstance().isDefaultSearchManaged();
                }
                return super.isPreferenceClickDisabledByPolicy(preference);
            }
        };
    }
}
