// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.DialogFragment;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.PasswordUIView;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPreferences;
import org.chromium.chrome.browser.signin.AccountAdder;
import org.chromium.chrome.browser.signin.AddGoogleAccountDialogFragment;
import org.chromium.chrome.browser.signin.AddGoogleAccountDialogFragment.AddGoogleAccountListener;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.ui.ChooseAccountFragment;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;

import java.util.List;

/**
 * The main settings screen, shown when the user first opens Settings.
 */
public class MainPreferences extends PreferenceFragment implements SignInStateObserver {

    public static final String PREF_SIGN_IN = "sign_in";
    public static final String PREF_SEARCH_ENGINE = "search_engine";
    public static final String PREF_DOCUMENT_MODE = "document_mode";
    public static final String PREF_AUTOFILL_SETTINGS = "autofill_settings";
    public static final String PREF_SAVED_PASSWORDS = "saved_passwords";
    public static final String PREF_HOMEPAGE = "homepage";
    public static final String PREF_DATA_REDUCTION = "data_reduction";

    public static final String ACCOUNT_PICKER_DIALOG_TAG = "account_picker_dialog_tag";
    public static final String EXTRA_SHOW_SEARCH_ENGINE_PICKER = "show_search_engine_picker";

    private SignInPreference mSignInPreference;
    private ManagedPreferenceDelegate mManagedPreferenceDelegate;

    private boolean mShowSearchEnginePicker;

    public MainPreferences() {
        setHasOptionsMenu(true);
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (savedInstanceState == null && getArguments() != null
                && getArguments().getBoolean(EXTRA_SHOW_SEARCH_ENGINE_PICKER, false)) {
            mShowSearchEnginePicker = true;
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        SigninManager.get(getActivity()).addSignInStateObserver(this);
        updatePreferences();

        if (mShowSearchEnginePicker) {
            mShowSearchEnginePicker = false;
            ((SearchEnginePreference) findPreference(PREF_SEARCH_ENGINE)).showDialog();
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        SigninManager.get(getActivity()).removeSignInStateObserver(this);
        unregisterSignInPref();
    }

    private void updatePreferences() {
        if (getPreferenceScreen() != null) getPreferenceScreen().removeAll();
        addPreferencesFromResource(R.xml.main_preferences);

        registerSignInPref();
        mSignInPreference.setOnPreferenceClickListener(new OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                if (!ChromeSigninController.get(getActivity()).isSignedIn()) {
                    displayAccountPicker();
                    return true;
                }
                return false;
            }
        });

        Preference documentMode = findPreference(PREF_DOCUMENT_MODE);
        if (FeatureUtilities.isDocumentModeEligible(getActivity())) {
            setOnOffSummary(documentMode,
                    !DocumentModeManager.getInstance(getActivity()).isOptedOutOfDocumentMode());
        } else {
            getPreferenceScreen().removePreference(documentMode);
        }

        ChromeBasePreference autofillPref =
                (ChromeBasePreference) findPreference(PREF_AUTOFILL_SETTINGS);
        setOnOffSummary(autofillPref, PersonalDataManager.isAutofillEnabled());
        autofillPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        ChromeBasePreference passwordsPref =
                (ChromeBasePreference) findPreference(PREF_SAVED_PASSWORDS);
        if (PasswordUIView.shouldUseSmartLockBranding()) {
            passwordsPref.setTitle(getResources().getString(
                    R.string.prefs_smart_lock_for_passwords));
        }
        setOnOffSummary(passwordsPref,
                PrefServiceBridge.getInstance().isRememberPasswordsEnabled());
        passwordsPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        Preference homepagePref = findPreference(PREF_HOMEPAGE);
        if (HomepageManager.shouldShowHomepageSetting()) {
            setOnOffSummary(homepagePref,
                    HomepageManager.getInstance(getActivity()).getPrefHomepageEnabled());
        } else {
            getPreferenceScreen().removePreference(homepagePref);
        }

        ChromeBasePreference dataReduction =
                (ChromeBasePreference) findPreference(PREF_DATA_REDUCTION);
        if (DataReductionProxySettings.getInstance().isDataReductionProxyAllowed()) {
            dataReduction.setSummary(
                    DataReductionPreferences.generateSummary(getResources()));
            dataReduction.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
        } else {
            getPreferenceScreen().removePreference(dataReduction);
        }
    }

    private void setOnOffSummary(Preference pref, boolean isOn) {
        pref.setSummary(getResources().getString(isOn ? R.string.text_on : R.string.text_off));
    }

    private void registerSignInPref() {
        unregisterSignInPref();
        mSignInPreference = (SignInPreference) findPreference(PREF_SIGN_IN);
        mSignInPreference.registerForUpdates();
    }

    private void unregisterSignInPref() {
        if (mSignInPreference != null) {
            mSignInPreference.unregisterForUpdates();
            mSignInPreference = null;
        }
    }

    private void displayAccountPicker() {
        displayAccountPicker(new Callback<DialogFragment>() {
            @Override
            public void onResult(DialogFragment fragment) {}
        });
    }

    /**
     * Displays the account picker or the add account dialog and signs the user in.
     *
     * @param callback Called with the fragment that was shown, or null. Used for testing.
     */
    @VisibleForTesting
    public void displayAccountPicker(final Callback<DialogFragment> callback) {
        Context context = getActivity();
        if (context == null) {
            postCallback(callback, null);
            return;
        }

        if (!SigninManager.get(context).isSignInAllowed()) {
            if (SigninManager.get(context).isSigninDisabledByPolicy()) {
                ManagedPreferencesUtils.showManagedByAdministratorToast(context);
            }
            postCallback(callback, null);
            return;
        }

        AccountManagerHelper.get(context).getGoogleAccountNames(new Callback<List<String>>() {
            @Override
            public void onResult(List<String> accountNames) {
                if (!accountNames.isEmpty()) {
                    if (getFragmentManager().findFragmentByTag(ACCOUNT_PICKER_DIALOG_TAG) != null) {
                        callback.onResult(null);
                    } else {
                        ChooseAccountFragment chooserFragment =
                                new ChooseAccountFragment(accountNames);
                        chooserFragment.show(getFragmentManager(), ACCOUNT_PICKER_DIALOG_TAG);
                        callback.onResult(chooserFragment);
                    }
                } else {
                    AddGoogleAccountDialogFragment dialog = new AddGoogleAccountDialogFragment();
                    dialog.setListener(new AddGoogleAccountListener() {
                        @Override
                        public void onAddAccountClicked() {
                            RecordUserAction.record("Signin_AddAccountToDevice");
                            AccountAdder.getInstance().addAccount(
                                    MainPreferences.this, AccountAdder.ADD_ACCOUNT_RESULT);
                        }
                    });
                    dialog.show(getFragmentManager(), null);
                    callback.onResult(dialog);
                }
            }
        });
    }

    private <V> void postCallback(final Callback<V> callback, final V result) {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                callback.onResult(result);
            }
        });
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
                return super.isPreferenceClickDisabledByPolicy(preference);
            }
        };
    }
}
