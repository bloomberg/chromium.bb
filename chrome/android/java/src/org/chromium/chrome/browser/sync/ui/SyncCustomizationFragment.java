// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.app.Dialog;
import android.app.DialogFragment;
import android.app.FragmentTransaction;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
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
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.child_accounts.ChildAccountService;
import org.chromium.chrome.browser.invalidation.InvalidationController;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.chrome.browser.sync.SyncStartupHelper;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.internal_api.pub.PassphraseType;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.signin.AccountManagerHelper;

import java.io.IOException;
import java.util.HashSet;
import java.util.Set;

/**
 * Settings fragment to customize Sync options (data types, encryption).
 */
public class SyncCustomizationFragment extends PreferenceFragment implements
        PassphraseDialogFragment.Listener, PassphraseTypeDialogFragment.Listener,
        OnPreferenceClickListener, OnPreferenceChangeListener,
        ProfileSyncService.SyncStateChangedListener {

    private static final String TAG = "SyncCustomizationFragment";

    @VisibleForTesting
    public static final String FRAGMENT_ENTER_PASSWORD = "enter_password";
    @VisibleForTesting
    public static final String FRAGMENT_CUSTOM_PASSWORD = "custom_password";
    @VisibleForTesting
    public static final String FRAGMENT_PASSWORD_TYPE = "password_type";
    private static final String FRAGMENT_SPINNER = "spinner";

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
    public static final String PREFERENCE_ENCRYPTION = "encryption";

    public static final String ARGUMENT_ACCOUNT = "account";

    private static final int ERROR_COLOR = Color.RED;
    private static final String PREF_SYNC_SWITCH = "sync_switch";
    private static final String PREFERENCE_SYNC_MANAGE_DATA = "sync_manage_data";

    private ChromeSwitchPreference mSyncSwitchPreference;
    private AndroidSyncSettings mAndroidSyncSettings;

    private AndroidSyncSettings.AndroidSyncSettingsObserver mAndroidSyncSettingsObserver;

    @VisibleForTesting
    public static final String[] PREFS_TO_SAVE = {
        PREFERENCE_SYNC_EVERYTHING,
        PREFERENCE_SYNC_AUTOFILL,
        PREFERENCE_SYNC_BOOKMARKS,
        PREFERENCE_SYNC_OMNIBOX,
        PREFERENCE_SYNC_PASSWORDS,
        PREFERENCE_SYNC_RECENT_TABS,
    };

    private static final String DASHBOARD_URL = "https://www.google.com/settings/chrome/sync";

    private SwitchPreference mSyncEverything;
    private CheckBoxPreference mSyncAutofill;
    private CheckBoxPreference mSyncBookmarks;
    private CheckBoxPreference mSyncOmnibox;
    private CheckBoxPreference mSyncPasswords;
    private CheckBoxPreference mSyncRecentTabs;
    private Preference mSyncEncryption;
    private Preference mManageSyncData;
    private CheckBoxPreference[] mAllTypes;
    private boolean mCheckboxesInitialized;

    /**
     * Transitional flag required to support clients that haven't been upgraded to keystore based
     * encryption. For these users, password sync cannot be disabled.
     */
    private boolean mPasswordSyncConfigurable;

    /**
     * Helper object that notifies us once sync startup has completed.
     */
    private SyncStartupHelper mSyncStartupHelper;

    private ProfileSyncService mProfileSyncService;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        mAndroidSyncSettings = AndroidSyncSettings.get(getActivity());
        mProfileSyncService = ProfileSyncService.get(getActivity());

        getActivity().setTitle(R.string.sign_in_sync);

        View view = super.onCreateView(inflater, container, savedInstanceState);
        addPreferencesFromResource(R.xml.sync_customization_preferences);
        mSyncEverything = (SwitchPreference) findPreference(PREFERENCE_SYNC_EVERYTHING);
        mSyncAutofill = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_AUTOFILL);
        mSyncBookmarks = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_BOOKMARKS);
        mSyncOmnibox = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_OMNIBOX);
        mSyncPasswords = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_PASSWORDS);
        mSyncRecentTabs = (CheckBoxPreference) findPreference(PREFERENCE_SYNC_RECENT_TABS);
        mSyncEncryption = findPreference(PREFERENCE_ENCRYPTION);
        mSyncEncryption.setOnPreferenceClickListener(this);
        mManageSyncData = findPreference(PREFERENCE_SYNC_MANAGE_DATA);
        mManageSyncData.setOnPreferenceClickListener(this);

        mAllTypes = new CheckBoxPreference[]{
            mSyncAutofill, mSyncBookmarks, mSyncOmnibox, mSyncPasswords, mSyncRecentTabs,
        };

        mSyncEverything.setOnPreferenceChangeListener(this);
        for (CheckBoxPreference type : mAllTypes) {
            type.setOnPreferenceChangeListener(this);
        }

        // Disable the UI until the sync backend is initialized.
        if (!mProfileSyncService.isSyncInitialized()) {
            // Display a loading dialog until showConfigure() is called.
            if (savedInstanceState == null) {
                displaySpinnerDialog();
            }
        }

        mSyncSwitchPreference = (ChromeSwitchPreference) findPreference(PREF_SYNC_SWITCH);
        boolean isSyncEnabled = mAndroidSyncSettings.isSyncEnabled();
        mSyncSwitchPreference.setChecked(isSyncEnabled);
        mSyncSwitchPreference.setEnabled(canDisableSync());
        mSyncSwitchPreference.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                assert canDisableSync();
                SyncController syncController = SyncController.get(getActivity());
                if ((boolean) newValue) {
                    syncController.start();
                } else {
                    syncController.stop();
                }
                new Handler().post(new Runnable() {
                    @Override
                    public void run() {
                        updateDataTypeState();
                    }
                });
                return true;
            }
        });

        mPasswordSyncConfigurable = mProfileSyncService.isSyncInitialized()
                && mProfileSyncService.isCryptographerReady();
        updateDataTypeState();

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
            new Handler().post(new Runnable() {
                @Override
                public void run() {
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
        return !ChildAccountService.getInstance(getActivity()).hasChildAccount();
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


    // Utility routine to dispose of our SyncStartupHelper once it is no longer
    // needed.
    private void destroySyncStartupHelper() {
        if (mSyncStartupHelper != null) {
            mSyncStartupHelper.destroy();
            mSyncStartupHelper = null;
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        // This flag prevents sync from actually syncing when the backend is initialized.
        mProfileSyncService.setSetupInProgress(true);
        mSyncStartupHelper = new SyncStartupHelper(
                mProfileSyncService,
                new SyncStartupHelper.Delegate() {
                    @Override
                    public void startupComplete() {
                        destroySyncStartupHelper();
                        showConfigure();
                    }

                    @Override
                    public void startupFailed() {
                        destroySyncStartupHelper();
                        closeDialogIfOpen(FRAGMENT_SPINNER);
                        getActivity().finish();
                    }
                });
        // Startup the sync backend. When sync is initialized, the startupComplete()
        // callback will be invoked to allow us to display our UI.
        mSyncStartupHelper.startSync();
        mProfileSyncService.addSyncStateChangedListener(this);
        // We need to act on Android sync state being changed from somewhere else.
        mAndroidSyncSettingsObserver =  new AndroidSyncSettingsObserver();
        mAndroidSyncSettings.registerObserver(mAndroidSyncSettingsObserver);
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mSyncStartupHelper != null) {
            destroySyncStartupHelper();
        }
        // Sync is already started. If we're closing this activity, apply our configuration
        // changes and tell sync that the user is done configuring sync.
        if (!getActivity().isChangingConfigurations()) {
            if (mSyncSwitchPreference.isChecked()) {
                // Save the new data type state.
                configureSyncDataTypes();
                // Inform sync that the user has finished setting up sync at least once.
                mProfileSyncService.setSyncSetupCompleted();
            } else {
                // Since we start the sync backend when we open this dialog, shut it down here.
                SyncController.get(getActivity()).stop();
            }
            // Setup is no longer in progress. This was preventing sync from actually running
            // while the backend was up.
            mProfileSyncService.setSetupInProgress(false);
        }
        if (mAndroidSyncSettingsObserver != null) {
            mAndroidSyncSettings.unregisterObserver(mAndroidSyncSettingsObserver);
        }
        mProfileSyncService.removeSyncStateChangedListener(this);
    }

    public void showConfigure() {
        closeDialogIfOpen(FRAGMENT_SPINNER);
        closeDialogIfOpen(FRAGMENT_CUSTOM_PASSWORD);
        closeDialogIfOpen(FRAGMENT_ENTER_PASSWORD);

        assert mProfileSyncService.isSyncInitialized();
        if (!mProfileSyncService.isSyncInitialized()) {
            // Asserts could be disabled on devices and there could be a crash when
            // toggling sync on/off. http://crbug.com/344524
            return;
        }
        // Clear out any previous passphrase error.
        updateEncryptionState();

        // We don't have any previously-saved checkbox states, so let's initialize from
        // the current sync state - does nothing if we already set the states, so we
        // don't clobber the user settings after the fragment is paused/resumed.
        Set<ModelType> syncTypes = mProfileSyncService.getPreferredDataTypes();
        if (!mCheckboxesInitialized) {
            mCheckboxesInitialized = true;
            mSyncEverything.setChecked(mProfileSyncService.hasKeepEverythingSynced());
            mSyncAutofill.setChecked(syncTypes.contains(ModelType.AUTOFILL));
            mSyncBookmarks.setChecked(syncTypes.contains(ModelType.BOOKMARK));
            mSyncOmnibox.setChecked(syncTypes.contains(ModelType.TYPED_URL));
            mSyncPasswords.setChecked(mPasswordSyncConfigurable
                    && syncTypes.contains(ModelType.PASSWORD));
            mSyncRecentTabs.setChecked(syncTypes.contains(ModelType.PROXY_TABS));
        }
        updateDataTypeState();
    }

    private void updateEncryptionState() {
        mSyncEncryption.setSummary(null);
        if (!mProfileSyncService.isSyncInitialized()) {
            // We can not inspect passphrase state if sync is not initialized. This case happens
            // whenever the user turns off sync from the customization screen. In this case, it
            // makes sense to leave the summary as empty, since we really have no idea.
            return;
        }
        if (mProfileSyncService.isPassphraseRequiredForDecryption() && isAdded()) {
            if (mProfileSyncService.isUsingSecondaryPassphrase()) {
                mSyncEncryption.setSummary(
                        errorSummary(getString(R.string.sync_need_passphrase)));
            } else {
                mSyncEncryption.setSummary(
                        errorSummary(getString(R.string.sync_need_password)));
            }
        }
        mPasswordSyncConfigurable = mProfileSyncService.isCryptographerReady();
    }

    /**
     * Applies a span to the given string to give it an error color.
     */
    private static Spannable errorSummary(String string) {
        SpannableString summary = new SpannableString(string);
        summary.setSpan(new ForegroundColorSpan(ERROR_COLOR), 0, summary.length(), 0);
        return summary;
    }

    private void configureSyncDataTypes() {
        if (maybeDisableSync()) return;

        boolean syncEverything = mSyncEverything.isChecked();
        mProfileSyncService.setPreferredDataTypes(syncEverything, getSelectedModelTypes());
        // Update the invalidation listener with the set of types we are enabling.
        InvalidationController invController =
                InvalidationController.get(getActivity());
        invController.setRegisteredTypes(getAccountArgument(), syncEverything,
                mProfileSyncService.getPreferredDataTypes());
    }

    private Set<ModelType> getSelectedModelTypes() {
        Set<ModelType> types = new HashSet<ModelType>();
        if (mSyncAutofill.isChecked()) types.add(ModelType.AUTOFILL);
        if (mSyncBookmarks.isChecked()) types.add(ModelType.BOOKMARK);
        if (mSyncOmnibox.isChecked()) types.add(ModelType.TYPED_URL);
        if (mSyncPasswords.isChecked()) {
            assert mPasswordSyncConfigurable;
            types.add(ModelType.PASSWORD);
        }
        if (mSyncRecentTabs.isChecked()) types.add(ModelType.PROXY_TABS);
        return types;
    }

    private void displayPasswordTypeDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseTypeDialogFragment dialog = PassphraseTypeDialogFragment.create(
                mProfileSyncService.getPassphraseType(),
                mProfileSyncService.getExplicitPassphraseTime(),
                mProfileSyncService.isEncryptEverythingAllowed());
        dialog.show(ft, FRAGMENT_PASSWORD_TYPE);
        dialog.setTargetFragment(this, -1);
    }

    private void displayPasswordDialog(boolean isGaia, boolean isUpdate) {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseDialogFragment.newInstance(this, isGaia, isUpdate)
                .show(ft, FRAGMENT_ENTER_PASSWORD);
    }

    private void displayCustomPasswordDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseCreationDialogFragment dialog = new PassphraseCreationDialogFragment();
        dialog.setTargetFragment(this, -1);
        dialog.show(ft, FRAGMENT_CUSTOM_PASSWORD);
    }

    private void displaySpinnerDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        SpinnerDialogFragment dialog = new SpinnerDialogFragment();
        dialog.setTargetFragment(this, 0);
        dialog.show(ft, FRAGMENT_SPINNER);
    }

    private void closeDialogIfOpen(String tag) {
        DialogFragment df = (DialogFragment) getFragmentManager().findFragmentByTag(tag);
        if (df != null) {
            df.dismiss();
        }
    }

    private void configureEncryption(String passphrase, boolean isGaia) {
        if (mProfileSyncService.isSyncInitialized()) {
            mProfileSyncService.enableEncryptEverything();
            mProfileSyncService.setEncryptionPassphrase(passphrase, isGaia);
            // Configure the current set of data types - this tells the sync engine to
            // apply our encryption configuration changes.
            configureSyncDataTypes();
            // Re-display our config UI to properly reflect the new state.
            showConfigure();
        }
    }

    private void handleEncryptWithGaia(final String passphrase) {
        AccountManager accountManager = (AccountManager) getActivity().getSystemService(
                Activity.ACCOUNT_SERVICE);
        String username = getArguments().getString(ARGUMENT_ACCOUNT);
        AccountManagerCallback<Bundle> callback = new AccountManagerCallback<Bundle>() {
            @Override
            public void run(AccountManagerFuture<Bundle> future) {
                boolean validPassword = false;
                try {
                    Bundle result = future.getResult();
                    validPassword = result.getBoolean(AccountManager.KEY_BOOLEAN_RESULT);
                } catch (OperationCanceledException e) {
                    // TODO(jgreenwald): notify user that we're unable to
                    // validate passphrase?
                    Log.e(TAG, "unable to verify password", e);
                } catch (AuthenticatorException e) {
                    Log.e(TAG, "unable to verify password", e);
                } catch (IOException e) {
                    Log.e(TAG, "unable to verify password", e);
                }

                Log.d(TAG, "GAIA password valid: " + validPassword);
                if (validPassword) {
                    configureEncryption(passphrase, true);
                } else {
                    notifyInvalidPassphrase();
                }
            }
        };
        Account account = AccountManagerHelper.createAccountFromName(username);
        Bundle options = new Bundle();
        options.putString(AccountManager.KEY_PASSWORD, passphrase);
        accountManager.confirmCredentials(account, options, null, callback, null);
    }

    private void handleEncryptWithCustomPassphrase(String passphrase) {
        configureEncryption(passphrase, false);
    }

    private void handleDecryption(String passphrase) {
        if (!passphrase.isEmpty() && mProfileSyncService.setDecryptionPassphrase(passphrase)) {
            // Update our configuration UI.
            showConfigure();
        } else {
            // Let the user know that the passphrase was not valid.
            notifyInvalidPassphrase();
        }
    }

    /**
     * Callback for PassphraseDialogFragment.Listener
     */
    @Override
    public void onPassphraseEntered(String passphrase, boolean isGaia, boolean isUpdate) {
        if (isUpdate) {
            if (isGaia) {
                handleEncryptWithGaia(passphrase);
            } else {
                handleEncryptWithCustomPassphrase(passphrase);
            }
        } else {
            handleDecryption(passphrase);
        }
    }

    private void notifyInvalidPassphrase() {
        PassphraseDialogFragment passwordDialog = (PassphraseDialogFragment)
                getFragmentManager().findFragmentByTag(FRAGMENT_ENTER_PASSWORD);
        if (passwordDialog != null) {
            passwordDialog.invalidPassphrase();
        } else {
            Log.w(TAG, "invalid passphrase but no dialog to notify");
        }
    }

    /**
     * Callback for PassphraseDialogFragment.Listener
     */
    @Override
    public void onPassphraseCanceled(boolean isGaia, boolean isUpdate) {
    }

    /**
     * Callback for PassphraseTypeDialogFragment.Listener
     */
    @Override
    public void onPassphraseTypeSelected(PassphraseType type) {
        boolean isAllDataEncrypted = mProfileSyncService.isEncryptEverythingEnabled();
        boolean isPassphraseGaia = !mProfileSyncService.isUsingSecondaryPassphrase();

        if (type == PassphraseType.IMPLICIT_PASSPHRASE) {
            // custom passphrase -> gaia is not allowed
            assert (isPassphraseGaia);
            boolean isGaia = true;
            boolean isUpdate = !isAllDataEncrypted;
            displayPasswordDialog(isGaia, isUpdate);
        } else if (type == PassphraseType.CUSTOM_PASSPHRASE) {
            if (isPassphraseGaia) {
                displayCustomPasswordDialog();
            } else {
                // Now using the existing custom passphrase to encrypt
                // everything.
                boolean isGaia = false;
                boolean isUpdate = false;
                displayPasswordDialog(isGaia, isUpdate);
            }
        }
    }

    /**
     * Callback for OnPreferenceClickListener
     */
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (!isResumed()) {
            // This event could come in after onPause if the user clicks back and the preference at
            // roughly the same time.  See http://b/5983282
            return false;
        }
        if (preference == mSyncEncryption && mProfileSyncService.isSyncInitialized()) {
            if (mProfileSyncService.isPassphraseRequiredForDecryption()) {
                displayPasswordDialog(!mProfileSyncService.isUsingSecondaryPassphrase(), false);
            } else {
                displayPasswordTypeDialog();
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

    private Account getAccountArgument() {
        return AccountManagerHelper
                .createAccountFromName(getArguments().getString(ARGUMENT_ACCOUNT));
    }

    private static class SpinnerDialogFragment extends DialogFragment {
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            ProgressDialog dialog = new ProgressDialog(getActivity());
            dialog.setMessage(getResources().getString(R.string.sync_loading));
            return dialog;
        }

        @Override
        public void onCancel(DialogInterface dialog) {
            super.onCancel(dialog);
            ((SyncCustomizationFragment) getTargetFragment()).getActivity().finish();
        }
    }

    private void updateDataTypeState() {
        boolean syncEnabled = mSyncSwitchPreference.isChecked();
        mSyncEverything.setEnabled(syncEnabled);
        mSyncEncryption.setEnabled(syncEnabled);

        boolean syncEverything = mSyncEverything.isChecked();
        for (CheckBoxPreference pref : mAllTypes) {
            boolean canSyncType = pref != mSyncPasswords || mPasswordSyncConfigurable;

            if (syncEverything) pref.setChecked(canSyncType);

            pref.setEnabled(syncEnabled && !syncEverything && canSyncType);
        }
    }

    @Override
    public void syncStateChanged() {
        updateEncryptionState();
        updateDataTypeState();
    }

    /**
     * Disables Sync if all data types have been disabled.
     * @return true if Sync has been disabled, false otherwise.
     */
    private boolean maybeDisableSync() {
        if (mSyncEverything.isChecked()
                || !getSelectedModelTypes().isEmpty()
                || !canDisableSync()) {
            return false;
        }

        mSyncSwitchPreference.setChecked(false);
        mSyncEverything.setChecked(true);
        updateDataTypeState();
        return true;
    }

    private class AndroidSyncSettingsObserver
            implements AndroidSyncSettings.AndroidSyncSettingsObserver {

        @Override
        public void androidSyncSettingsChanged() {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    // Sync can be turned off externally (for example from the Google Dashboard)
                    // while this fragment is showing, so we need to act on that.
                    boolean enabled = mAndroidSyncSettings.isSyncEnabled();
                    mSyncSwitchPreference.setChecked(enabled);
                }
            });
        }
    }
}
