// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.accounts.Account;
import android.app.DialogFragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;
import android.preference.SwitchPreference;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.childaccounts.ChildAccountService;
import org.chromium.chrome.browser.invalidation.InvalidationController;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.SyncedAccountPreference;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.SyncAccountSwitcher;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.ModelType;
import org.chromium.sync.PassphraseType;
import org.chromium.sync.StopSource;
import org.chromium.sync.signin.AccountManagerHelper;

import java.util.HashSet;
import java.util.Set;

/**
 * Settings fragment to customize Sync options (data types, encryption).
 */
public class SyncCustomizationFragment extends PreferenceFragment
        implements PassphraseDialogFragment.Listener, PassphraseCreationDialogFragment.Listener,
                   PassphraseTypeDialogFragment.Listener, OnPreferenceClickListener,
                   OnPreferenceChangeListener, ProfileSyncService.SyncStateChangedListener {
    private static final String TAG = "SyncCustomizationFragment";

    @VisibleForTesting
    public static final String FRAGMENT_ENTER_PASSPHRASE = "enter_password";
    @VisibleForTesting
    public static final String FRAGMENT_CUSTOM_PASSPHRASE = "custom_password";
    @VisibleForTesting
    public static final String FRAGMENT_PASSPHRASE_TYPE = "password_type";
    @VisibleForTesting
    public static final String PREFERENCE_SYNC_EVERYTHING = "sync_everything";
    @VisibleForTesting
    public static final String PREFERENCE_SYNC_AUTOFILL = "sync_autofill";
    @VisibleForTesting
    public static final String PREFERENCE_SYNC_BOOKMARKS = "sync_bookmarks";
    @VisibleForTesting
    public static final String PREFERENCE_SYNC_OMNIBOX = "sync_omnibox";
    @VisibleForTesting
    public static final String PREFERENCE_SYNC_PASSWORDS = "sync_passwords";
    @VisibleForTesting
    public static final String PREFERENCE_SYNC_RECENT_TABS = "sync_recent_tabs";
    @VisibleForTesting
    public static final String PREFERENCE_SYNC_SETTINGS = "sync_settings";
    @VisibleForTesting
    public static final String PREFERENCE_PAYMENTS_INTEGRATION = "payments_integration";
    @VisibleForTesting
    public static final String PREFERENCE_ENCRYPTION = "encryption";
    @VisibleForTesting
    public static final String PREF_SYNC_SWITCH = "sync_switch";
    @VisibleForTesting
    public static final String PREFERENCE_SYNC_MANAGE_DATA = "sync_manage_data";
    @VisibleForTesting
    public static final String PREFERENCE_SYNC_ACCOUNT_LIST = "synced_account";

    public static final String ARGUMENT_ACCOUNT = "account";

    private ChromeSwitchPreference mSyncSwitchPreference;
    private boolean mIsBackendInitialized;
    private boolean mIsPassphraseRequired;

    @VisibleForTesting
    public static final String[] PREFS_TO_SAVE = {
        PREFERENCE_SYNC_EVERYTHING,
        PREFERENCE_SYNC_AUTOFILL,
        PREFERENCE_SYNC_BOOKMARKS,
        PREFERENCE_SYNC_OMNIBOX,
        PREFERENCE_SYNC_PASSWORDS,
        PREFERENCE_SYNC_RECENT_TABS,
        PREFERENCE_SYNC_SETTINGS,
        PREFERENCE_PAYMENTS_INTEGRATION
    };

    private static final String DASHBOARD_URL = "https://www.google.com/settings/chrome/sync";

    private SwitchPreference mSyncEverything;
    private CheckBoxPreference mSyncAutofill;
    private CheckBoxPreference mSyncBookmarks;
    private CheckBoxPreference mSyncOmnibox;
    private CheckBoxPreference mSyncPasswords;
    private CheckBoxPreference mSyncRecentTabs;
    private CheckBoxPreference mSyncSettings;
    private CheckBoxPreference mPaymentsIntegration;
    private Preference mSyncEncryption;
    private Preference mManageSyncData;
    private CheckBoxPreference[] mAllTypes;
    private SyncedAccountPreference mSyncedAccountPreference;

    private ProfileSyncService mProfileSyncService;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mProfileSyncService = ProfileSyncService.get();
        assert mProfileSyncService != null;
        mIsBackendInitialized = mProfileSyncService.isBackendInitialized();
        mIsPassphraseRequired =
                mIsBackendInitialized && mProfileSyncService.isPassphraseRequiredForDecryption();

        getActivity().setTitle(R.string.sign_in_sync);

        View view = super.onCreateView(inflater, container, savedInstanceState);
        addPreferencesFromResource(R.xml.sync_customization_preferences);
        mSyncEverything = (SwitchPreference) findPreference(PREFERENCE_SYNC_EVERYTHING);
        mSyncAutofill = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_AUTOFILL);
        mSyncBookmarks = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_BOOKMARKS);
        mSyncOmnibox = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_OMNIBOX);
        mSyncPasswords = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_PASSWORDS);
        mSyncRecentTabs = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_RECENT_TABS);
        mSyncSettings = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_SETTINGS);
        mPaymentsIntegration = (CheckBoxPreference) findPreference(PREFERENCE_PAYMENTS_INTEGRATION);

        mSyncEncryption = findPreference(PREFERENCE_ENCRYPTION);
        mSyncEncryption.setOnPreferenceClickListener(this);
        mManageSyncData = findPreference(PREFERENCE_SYNC_MANAGE_DATA);
        mManageSyncData.setOnPreferenceClickListener(this);

        mAllTypes = new CheckBoxPreference[] {
                mSyncAutofill, mSyncBookmarks, mSyncOmnibox, mSyncPasswords,
                mSyncRecentTabs, mSyncSettings, mPaymentsIntegration
        };

        mSyncEverything.setOnPreferenceChangeListener(this);
        for (CheckBoxPreference type : mAllTypes) {
            type.setOnPreferenceChangeListener(this);
        }

        mSyncSwitchPreference = (ChromeSwitchPreference) findPreference(PREF_SYNC_SWITCH);
        mSyncSwitchPreference.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                assert canDisableSync();
                if ((boolean) newValue) {
                    mProfileSyncService.requestStart();
                } else {
                    stopSync();
                }
                // Must be done asynchronously because the switch state isn't updated
                // until after this function exits.
                new Handler().post(new Runnable() {
                    @Override
                    public void run() {
                        updateSyncStateFromSwitch();
                    }
                });
                return true;
            }
        });

        mSyncedAccountPreference =
                (SyncedAccountPreference) findPreference(PREFERENCE_SYNC_ACCOUNT_LIST);
        mSyncedAccountPreference.setOnPreferenceChangeListener(
                new SyncAccountSwitcher(getActivity(), mSyncedAccountPreference));

        return view;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (preference == mSyncEverything) {
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    updateDataTypeState();
                }
            });
            return true;
        }
        if (isSyncTypePreference(preference)) {
            final boolean syncAutofillToggled = preference == mSyncAutofill;
            final boolean preferenceChecked = (boolean) newValue;
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    if (syncAutofillToggled) {
                        // If the user checks the autofill sync checkbox, then enable and check the
                        // payments integration checkbox.
                        //
                        // If the user unchecks the autofill sync checkbox, then disable and uncheck
                        // the payments integration checkbox.
                        mPaymentsIntegration.setEnabled(preferenceChecked);
                        mPaymentsIntegration.setChecked(preferenceChecked);
                    }
                    maybeDisableSync();
                }
            });
            return true;
        }
        return false;
    }

    /**
     * @return Whether Sync can be disabled.
     */
    private boolean canDisableSync() {
        return !ChildAccountService.isChildAccount();
    }

    private boolean isSyncTypePreference(Preference preference) {
        for (Preference pref : mAllTypes) {
            if (pref == preference) return true;
        }
        return false;
    }

    /**
     * Returns the sync action bar switch to enable/disable sync.
     *
     * @return the mActionBarSwitch
     */
    @VisibleForTesting
    public ChromeSwitchPreference getSyncSwitchPreference() {
        return mSyncSwitchPreference;
    }

    @Override
    public void onStart() {
        super.onStart();
        // The current account may have been switched on a different screen so ensure the synced
        // account preference displays the correct signed in account.
        mSyncedAccountPreference.update();

        mIsBackendInitialized = mProfileSyncService.isBackendInitialized();
        mIsPassphraseRequired =
                mIsBackendInitialized && mProfileSyncService.isPassphraseRequiredForDecryption();
        // This prevents sync from actually syncing until the dialog is closed.
        mProfileSyncService.setSetupInProgress(true);
        mProfileSyncService.addSyncStateChangedListener(this);
        updateSyncState();
    }

    @Override
    public void onStop() {
        super.onStop();

        mProfileSyncService.removeSyncStateChangedListener(this);
        // If this activity is closing, apply configuration changes and tell sync that
        // the user is done configuring sync.
        if (!getActivity().isChangingConfigurations()) {
            // Only save state if the switch and external state match. If a stop and clear comes
            // while the dialog is open, this will be false and settings won't be saved.
            if (mSyncSwitchPreference.isChecked()
                    && AndroidSyncSettings.isSyncEnabled(getActivity())) {
                // Save the new data type state.
                configureSyncDataTypes();
                // Inform sync that the user has finished setting up sync at least once.
                mProfileSyncService.setFirstSetupComplete();
            }
            PersonalDataManager.setPaymentsIntegrationEnabled(mPaymentsIntegration.isChecked());
            // Setup is done. This was preventing sync from turning on even if it was enabled.
            // TODO(crbug/557784): This needs to be set only when we think the user is done with
            // setting up. This means: 1) If the user leaves the Sync Settings screen (via back)
            // or, 2) If the user leaves the screen by tapping on "Manage Synced Data"
            mProfileSyncService.setSetupInProgress(false);
        }
    }

    /**
     * Update the state of all settings from sync.
     *
     * This sets the state of the sync switch from external sync state and then calls
     * updateSyncStateFromSwitch, which uses that as its source of truth.
     */
    private void updateSyncState() {
        boolean isSyncEnabled = AndroidSyncSettings.isSyncEnabled(getActivity());
        mSyncSwitchPreference.setChecked(isSyncEnabled);
        mSyncSwitchPreference.setEnabled(canDisableSync());
        updateSyncStateFromSwitch();
    }

    private void updateSyncAccountsListState() {
        SyncedAccountPreference accountList =
                (SyncedAccountPreference) findPreference(PREFERENCE_SYNC_ACCOUNT_LIST);

        // We remove the the SyncedAccountPreference if there's only 1 account on the device, so
        // it's possible for accountList to be null
        if (accountList != null) {
            Account[] accounts = AccountManagerHelper.get(getActivity()).getGoogleAccounts();
            if (accounts.length <= 1) {
                getPreferenceScreen().removePreference(accountList);
            } else {
                accountList.setEnabled(mSyncSwitchPreference.isChecked());
            }
        }
    }

    /**
     * Update the state of settings using the switch state to determine if sync is enabled.
     */
    private void updateSyncStateFromSwitch() {
        updateSyncEverythingState();
        updateDataTypeState();
        updateEncryptionState();
        updateSyncAccountsListState();
    }

    /**
     * Update the encryption state.
     *
     * If sync's backend is initialized, the button is enabled and the dialog will present the
     * valid encryption options for the user. Otherwise, any encryption dialogs will be closed
     * and the button will be disabled because the backend is needed in order to know and
     * modify the encryption state.
     */
    private void updateEncryptionState() {
        boolean isSyncEnabled = mSyncSwitchPreference.isChecked();
        boolean isBackendInitialized = mProfileSyncService.isBackendInitialized();
        mSyncEncryption.setEnabled(isSyncEnabled && isBackendInitialized);
        mSyncEncryption.setSummary(null);
        if (!isBackendInitialized) {
            // If sync is not initialized, encryption state is unavailable and can't be changed.
            // Leave the button disabled and the summary empty. Additionally, close the dialogs in
            // case they were open when a stop and clear comes.
            closeDialogIfOpen(FRAGMENT_CUSTOM_PASSPHRASE);
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
            return;
        }
        if (!mProfileSyncService.isPassphraseRequiredForDecryption()) {
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
        }
        if (mProfileSyncService.isPassphraseRequiredForDecryption() && isAdded()) {
            mSyncEncryption.setSummary(
                    errorSummary(getString(R.string.sync_need_passphrase), getActivity()));
        }
    }

    /**
     * Applies a span to the given string to give it an error color.
     */
    private static Spannable errorSummary(String string, Context context) {
        SpannableString summary = new SpannableString(string);
        summary.setSpan(new ForegroundColorSpan(
                ApiCompatibilityUtils.getColor(
                        context.getResources(), R.color.input_underline_error_color)),
                0, summary.length(), 0);
        return summary;
    }

    private void configureSyncDataTypes() {
        if (maybeDisableSync()) return;

        boolean syncEverything = mSyncEverything.isChecked();
        mProfileSyncService.setPreferredDataTypes(syncEverything, getSelectedModelTypes());
        // Update the invalidation listener with the set of types we are enabling.
        InvalidationController invController = InvalidationController.get(getActivity());
        invController.ensureStartedAndUpdateRegisteredTypes();
    }

    private Set<Integer> getSelectedModelTypes() {
        Set<Integer> types = new HashSet<Integer>();
        if (mSyncAutofill.isChecked()) types.add(ModelType.AUTOFILL);
        if (mSyncBookmarks.isChecked()) types.add(ModelType.BOOKMARKS);
        if (mSyncOmnibox.isChecked()) types.add(ModelType.TYPED_URLS);
        if (mSyncPasswords.isChecked()) types.add(ModelType.PASSWORDS);
        if (mSyncRecentTabs.isChecked()) types.add(ModelType.PROXY_TABS);
        if (mSyncSettings.isChecked()) types.add(ModelType.PREFERENCES);
        return types;
    }

    private void displayPassphraseTypeDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseTypeDialogFragment dialog = PassphraseTypeDialogFragment.create(
                mProfileSyncService.getPassphraseType(),
                mProfileSyncService.getExplicitPassphraseTime(),
                mProfileSyncService.isEncryptEverythingAllowed());
        dialog.show(ft, FRAGMENT_PASSPHRASE_TYPE);
        dialog.setTargetFragment(this, -1);
    }

    private void displayPassphraseDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseDialogFragment.newInstance(this).show(ft, FRAGMENT_ENTER_PASSPHRASE);
    }

    private void displayCustomPassphraseDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseCreationDialogFragment dialog = new PassphraseCreationDialogFragment();
        dialog.setTargetFragment(this, -1);
        dialog.show(ft, FRAGMENT_CUSTOM_PASSPHRASE);
    }

    private void closeDialogIfOpen(String tag) {
        FragmentManager manager = getFragmentManager();
        if (manager == null) {
            // Do nothing if the manager doesn't exist yet; see http://crbug.com/480544.
            return;
        }
        DialogFragment df = (DialogFragment) manager.findFragmentByTag(tag);
        if (df != null) {
            df.dismiss();
        }
    }

    private void configureEncryption(String passphrase) {
        if (mProfileSyncService.isBackendInitialized()) {
            mProfileSyncService.enableEncryptEverything();
            mProfileSyncService.setEncryptionPassphrase(passphrase);
            // Configure the current set of data types - this tells the sync engine to
            // apply our encryption configuration changes.
            configureSyncDataTypes();
            // Re-display our config UI to properly reflect the new state.
            updateSyncState();
        }
    }

    /**
     * @return whether the passphrase successfully decrypted the pending keys.
     */
    private boolean handleDecryption(String passphrase) {
        if (!passphrase.isEmpty() && mProfileSyncService.setDecryptionPassphrase(passphrase)) {
            // PassphraseDialogFragment doesn't handle closing itself, so do it here. This is
            // not done in updateSyncState() because that happens onResume and possibly in other
            // cases where the dialog should stay open.
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
            // Update our configuration UI.
            updateSyncState();
            return true;
        }
        return false;
    }

    /**
     * Callback for PassphraseDialogFragment.Listener
     */
    @Override
    public boolean onPassphraseEntered(String passphrase) {
        if (!mProfileSyncService.isBackendInitialized()) {
            // If the backend was shut down since the dialog was opened, do nothing.
            return false;
        }
        return handleDecryption(passphrase);
    }

    /**
     * Callback for PassphraseDialogFragment.Listener
     */
    @Override
    public void onPassphraseCanceled() {
    }

    /**
     * Callback for PassphraseCreationDialogFragment.Listener
     */
    @Override
    public void onPassphraseCreated(String passphrase) {
        if (!mProfileSyncService.isBackendInitialized()) {
            // If the backend was shut down since the dialog was opened, do nothing.
            return;
        }
        configureEncryption(passphrase);
    }

    /**
     * Callback for PassphraseTypeDialogFragment.Listener
     */
    @Override
    public void onPassphraseTypeSelected(PassphraseType type) {
        if (!mProfileSyncService.isBackendInitialized()) {
            // If the backend was shut down since the dialog was opened, do nothing.
            return;
        }

        boolean isAllDataEncrypted = mProfileSyncService.isEncryptEverythingEnabled();
        boolean isUsingSecondaryPassphrase = mProfileSyncService.isUsingSecondaryPassphrase();

        // The passphrase type should only ever be selected if the account doesn't have
        // full encryption enabled. Otherwise both options should be disabled.
        assert !isAllDataEncrypted;
        assert !isUsingSecondaryPassphrase;
        displayCustomPassphraseDialog();
    }

    /**
     * Callback for OnPreferenceClickListener
     */
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (!isResumed()) {
            // This event could come in after onPause if the user clicks back and the preference at
            // roughly the same time. See http://b/5983282
            return false;
        }
        if (preference == mSyncEncryption && mProfileSyncService.isBackendInitialized()) {
            if (mProfileSyncService.isPassphraseRequiredForDecryption()) {
                displayPassphraseDialog();
            } else {
                displayPassphraseTypeDialog();
                return true;
            }
        } else if (preference == mManageSyncData) {
            openDashboardTabInNewActivityStack();
            return true;
        }
        return false;
    }

    /**
     * Opens the Google Dashboard where the user can control the data stored for the account.
     */
    private void openDashboardTabInNewActivityStack() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(DASHBOARD_URL));
        intent.setPackage(getActivity().getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
    }

    /**
     * Update the state of the sync everything switch.
     *
     * If sync is on, load the pref from native. Otherwise display sync everything as on but
     * disable the switch.
     */
    private void updateSyncEverythingState() {
        boolean isSyncEnabled = mSyncSwitchPreference.isChecked();
        mSyncEverything.setEnabled(isSyncEnabled);
        mSyncEverything.setChecked(!isSyncEnabled
                || mProfileSyncService.hasKeepEverythingSynced());
    }

    /**
     * Update the data type switch state.
     *
     * If sync is on, load the prefs from native. Otherwise, all data types are disabled and
     * checked. Note that the Password data type will be shown as disabled and unchecked between
     * sync being turned on and the backend initialization completing.
     */
    private void updateDataTypeState() {
        boolean isSyncEnabled = mSyncSwitchPreference.isChecked();
        boolean syncEverything = mSyncEverything.isChecked();
        boolean passwordSyncConfigurable = mProfileSyncService.isBackendInitialized()
                && mProfileSyncService.isCryptographerReady();
        Set<Integer> syncTypes = mProfileSyncService.getPreferredDataTypes();
        boolean syncAutofill = syncTypes.contains(ModelType.AUTOFILL);
        for (CheckBoxPreference pref : mAllTypes) {
            boolean canSyncType = true;
            if (pref == mSyncPasswords) canSyncType = passwordSyncConfigurable;
            if (pref == mPaymentsIntegration) canSyncType = syncAutofill;

            if (!isSyncEnabled) {
                pref.setChecked(true);
            } else if (syncEverything) {
                pref.setChecked(canSyncType);
            }

            pref.setEnabled(isSyncEnabled && !syncEverything && canSyncType);
        }
        if (isSyncEnabled && !syncEverything) {
            mSyncAutofill.setChecked(syncAutofill);
            mSyncBookmarks.setChecked(syncTypes.contains(ModelType.BOOKMARKS));
            mSyncOmnibox.setChecked(syncTypes.contains(ModelType.TYPED_URLS));
            mSyncPasswords.setChecked(passwordSyncConfigurable
                    && syncTypes.contains(ModelType.PASSWORDS));
            mSyncRecentTabs.setChecked(syncTypes.contains(ModelType.PROXY_TABS));
            // TODO(zea): Switch this to PREFERENCE once that datatype is
            // supported on Android.
            mSyncSettings.setChecked(syncTypes.contains(ModelType.PRIORITY_PREFERENCES));
            mPaymentsIntegration.setChecked(
                    syncAutofill && PersonalDataManager.isPaymentsIntegrationEnabled());
        }
    }

    /**
     * Listen to sync state changes.
     *
     * If the user has just turned on sync, this listener is needed in order to enable
     * the encryption settings once the backend has initialized.
     */
    @Override
    public void syncStateChanged() {
        boolean wasSyncInitialized = mIsBackendInitialized;
        boolean wasPassphraseRequired = mIsPassphraseRequired;
        mIsBackendInitialized = mProfileSyncService.isBackendInitialized();
        mIsPassphraseRequired =
                mIsBackendInitialized && mProfileSyncService.isPassphraseRequiredForDecryption();
        if (mIsBackendInitialized != wasSyncInitialized
                || mIsPassphraseRequired != wasPassphraseRequired) {
            // Update all because Password syncability is also affected by the backend.
            updateSyncStateFromSwitch();
        }
    }

    /**
     * Disables Sync if all data types have been disabled.
     *
     * @return true if Sync has been disabled, false otherwise.
     */
    private boolean maybeDisableSync() {
        if (mSyncEverything.isChecked()
                || !getSelectedModelTypes().isEmpty()
                || !canDisableSync()) {
            return false;
        }
        stopSync();
        mSyncSwitchPreference.setChecked(false);
        // setChecked doesn't trigger the callback, so update manually.
        updateSyncStateFromSwitch();
        return true;
    }

    private void stopSync() {
        if (mProfileSyncService.isSyncRequested()) {
            RecordHistogram.recordEnumeratedHistogram("Sync.StopSource",
                    StopSource.CHROME_SYNC_SETTINGS, StopSource.STOP_SOURCE_LIMIT);
            mProfileSyncService.requestStop();
        }
    }
}
