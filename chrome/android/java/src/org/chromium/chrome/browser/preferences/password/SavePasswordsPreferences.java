// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceCategory;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.PasswordManagerHandler;
import org.chromium.chrome.browser.PasswordUIView;
import org.chromium.chrome.browser.SavedPasswordEntry;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegate;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.TextMessagePreference;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.Toast;

/**
 * The "Save passwords" screen in Settings, which allows the user to enable or disable password
 * saving, to view saved passwords (just the username and URL), and to delete saved passwords.
 */
public class SavePasswordsPreferences
        extends PreferenceFragment implements PasswordManagerHandler.PasswordListObserver,
                                              Preference.OnPreferenceClickListener {
    // Keys for name/password dictionaries.
    public static final String PASSWORD_LIST_URL = "url";
    public static final String PASSWORD_LIST_NAME = "name";
    public static final String PASSWORD_LIST_PASSWORD = "password";

    // Used to pass the password id into a new activity.
    public static final String PASSWORD_LIST_ID = "id";

    // The key for saving |mExportRequested| to instance bundle.
    private static final String SAVED_STATE_EXPORT_REQUESTED = "saved-state-export-requested";

    public static final String PREF_SAVE_PASSWORDS_SWITCH = "save_passwords_switch";
    public static final String PREF_AUTOSIGNIN_SWITCH = "autosignin_switch";

    private static final String PREF_KEY_CATEGORY_SAVED_PASSWORDS = "saved_passwords";
    private static final String PREF_KEY_CATEGORY_EXCEPTIONS = "exceptions";
    private static final String PREF_KEY_MANAGE_ACCOUNT_LINK = "manage_account_link";
    private static final String PREF_KEY_SAVED_PASSWORDS_NO_TEXT = "saved_passwords_no_text";

    // Name of the feature controlling the password export functionality.
    private static final String EXPORT_PASSWORDS = "PasswordExport";

    private static final int ORDER_SWITCH = 0;
    private static final int ORDER_AUTO_SIGNIN_CHECKBOX = 1;
    private static final int ORDER_MANAGE_ACCOUNT_LINK = 2;
    private static final int ORDER_SAVED_PASSWORDS = 3;
    private static final int ORDER_EXCEPTIONS = 4;
    private static final int ORDER_SAVED_PASSWORDS_NO_TEXT = 5;

    private boolean mNoPasswords;
    private boolean mNoPasswordExceptions;
    // True if the user triggered the password export flow and this fragment is waiting for the
    // result of the user's reauthentication.
    private boolean mExportRequested;
    private Preference mLinkPref;
    private ChromeSwitchPreference mSavePasswordsSwitch;
    private ChromeBaseCheckBoxPreference mAutoSignInSwitch;
    private TextMessagePreference mEmptyView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.prefs_saved_passwords);
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getActivity()));
        PasswordManagerHandlerProvider.getInstance().addObserver(this);
        if (ChromeFeatureList.isEnabled(EXPORT_PASSWORDS)
                && ReauthenticationManager.isReauthenticationApiAvailable()) {
            setHasOptionsMenu(true);
        }
        if (savedInstanceState != null
                && savedInstanceState.containsKey(SAVED_STATE_EXPORT_REQUESTED)) {
            mExportRequested =
                    savedInstanceState.getBoolean(SAVED_STATE_EXPORT_REQUESTED, mExportRequested);
        }
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.save_password_preferences_action_bar_menu, menu);
        menu.findItem(R.id.export_passwords).setEnabled(false);
    }

    @Override
    public void onPrepareOptionsMenu(Menu menu) {
        menu.findItem(R.id.export_passwords).setEnabled(!mNoPasswords);
        super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.export_passwords) {
            if (!ReauthenticationManager.isScreenLockSetUp(getActivity().getApplicationContext())) {
                Toast.makeText(getActivity().getApplicationContext(),
                             R.string.password_export_set_lock_screen, Toast.LENGTH_LONG)
                        .show();
            } else if (ReauthenticationManager.authenticationStillValid()) {
                exportAfterReauth();
            } else {
                mExportRequested = true;
                ReauthenticationManager.displayReauthenticationFragment(
                        R.string.lockscreen_description_export, getView().getId(),
                        getFragmentManager());
            }
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    /** Continues with the password export flow after the user successfully reauthenticated. */
    private void exportAfterReauth() {
        ExportWarningDialogFragment exportWarningDialogFragment = new ExportWarningDialogFragment();
        exportWarningDialogFragment.setExportWarningHandler(new DialogInterface.OnClickListener() {
            /** On positive button response asks the parent to continue with the export flow. */
            @Override
            public void onClick(DialogInterface dialog, int which) {
                if (which == AlertDialog.BUTTON_POSITIVE) {
                    exportAfterWarning();
                }
            }
        });
        exportWarningDialogFragment.show(getFragmentManager(), null);
    }

    private void exportAfterWarning() {
        // TODO(crbug.com/788701): Start the export.
    }

    /**
     * Empty screen message when no passwords or exceptions are stored.
     */
    private void displayEmptyScreenMessage() {
        mEmptyView = new TextMessagePreference(getActivity(), null);
        mEmptyView.setSummary(R.string.saved_passwords_none_text);
        mEmptyView.setKey(PREF_KEY_SAVED_PASSWORDS_NO_TEXT);
        mEmptyView.setOrder(ORDER_SAVED_PASSWORDS_NO_TEXT);
        getPreferenceScreen().addPreference(mEmptyView);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        ReauthenticationManager.setLastReauthTimeMillis(0);
    }

    void rebuildPasswordLists() {
        mNoPasswords = false;
        mNoPasswordExceptions = false;
        getPreferenceScreen().removeAll();
        createSavePasswordsSwitch();
        createAutoSignInCheckbox();
        PasswordManagerHandlerProvider.getInstance()
                .getPasswordManagerHandler()
                .updatePasswordLists();
    }

    /**
     * Removes the UI displaying the list of saved passwords or exceptions.
     * @param preferenceCategoryKey The key string identifying the PreferenceCategory to be removed.
     */
    private void resetList(String preferenceCategoryKey) {
        PreferenceCategory profileCategory =
                (PreferenceCategory) getPreferenceScreen().findPreference(preferenceCategoryKey);
        if (profileCategory != null) {
            profileCategory.removeAll();
            getPreferenceScreen().removePreference(profileCategory);
        }
    }

    /**
     * Removes the message informing the user that there are no saved entries to display.
     */
    private void resetNoEntriesTextMessage() {
        Preference message = getPreferenceScreen().findPreference(PREF_KEY_SAVED_PASSWORDS_NO_TEXT);
        if (message != null) {
            getPreferenceScreen().removePreference(message);
        }
    }

    @Override
    public void passwordListAvailable(int count) {
        resetList(PREF_KEY_CATEGORY_SAVED_PASSWORDS);
        resetNoEntriesTextMessage();

        mNoPasswords = count == 0;
        if (mNoPasswords) {
            if (mNoPasswordExceptions) displayEmptyScreenMessage();
            return;
        }

        displayManageAccountLink();

        PreferenceCategory profileCategory = new PreferenceCategory(getActivity());
        profileCategory.setKey(PREF_KEY_CATEGORY_SAVED_PASSWORDS);
        profileCategory.setTitle(R.string.section_saved_passwords);
        profileCategory.setOrder(ORDER_SAVED_PASSWORDS);
        getPreferenceScreen().addPreference(profileCategory);
        for (int i = 0; i < count; i++) {
            SavedPasswordEntry saved = PasswordManagerHandlerProvider.getInstance()
                                               .getPasswordManagerHandler()
                                               .getSavedPasswordEntry(i);
            PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getActivity());
            String url = saved.getUrl();
            String name = saved.getUserName();
            String password = saved.getPassword();
            screen.setTitle(url);
            screen.setOnPreferenceClickListener(this);
            screen.setSummary(name);
            Bundle args = screen.getExtras();
            args.putString(PASSWORD_LIST_NAME, name);
            args.putString(PASSWORD_LIST_URL, url);
            args.putString(PASSWORD_LIST_PASSWORD, password);
            args.putInt(PASSWORD_LIST_ID, i);
            profileCategory.addPreference(screen);
        }
    }

    @Override
    public void passwordExceptionListAvailable(int count) {
        resetList(PREF_KEY_CATEGORY_EXCEPTIONS);
        resetNoEntriesTextMessage();

        mNoPasswordExceptions = count == 0;
        if (mNoPasswordExceptions) {
            if (mNoPasswords) displayEmptyScreenMessage();
            return;
        }

        displayManageAccountLink();

        PreferenceCategory profileCategory = new PreferenceCategory(getActivity());
        profileCategory.setKey(PREF_KEY_CATEGORY_EXCEPTIONS);
        profileCategory.setTitle(R.string.section_saved_passwords_exceptions);
        profileCategory.setOrder(ORDER_EXCEPTIONS);
        getPreferenceScreen().addPreference(profileCategory);
        for (int i = 0; i < count; i++) {
            String exception = PasswordManagerHandlerProvider.getInstance()
                                       .getPasswordManagerHandler()
                                       .getSavedPasswordException(i);
            PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getActivity());
            screen.setTitle(exception);
            screen.setOnPreferenceClickListener(this);
            Bundle args = screen.getExtras();
            args.putString(PASSWORD_LIST_URL, exception);
            args.putInt(PASSWORD_LIST_ID, i);
            profileCategory.addPreference(screen);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mExportRequested) {
            mExportRequested = false;
            if (ReauthenticationManager.authenticationStillValid()) exportAfterReauth();
        }
        rebuildPasswordLists();
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(SAVED_STATE_EXPORT_REQUESTED, mExportRequested);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        PasswordManagerHandlerProvider.getInstance().removeObserver(this);
    }

    /**
     *  Preference was clicked. Either navigate to manage account site or launch the PasswordEditor
     *  depending on which preference it was.
     */
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference == mLinkPref) {
            Intent intent = new Intent(
                    Intent.ACTION_VIEW, Uri.parse(PasswordUIView.getAccountDashboardURL()));
            intent.setPackage(getActivity().getPackageName());
            getActivity().startActivity(intent);
        } else {
            // Launch preference activity with PasswordEntryEditor fragment with
            // intent extras specifying the object.
            Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                    getActivity(), PasswordEntryEditor.class.getName());
            intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, preference.getExtras());
            startActivity(intent);
        }
        return true;
    }

    private void createSavePasswordsSwitch() {
        mSavePasswordsSwitch = new ChromeSwitchPreference(getActivity(), null);
        mSavePasswordsSwitch.setKey(PREF_SAVE_PASSWORDS_SWITCH);
        mSavePasswordsSwitch.setTitle(R.string.prefs_saved_passwords);
        mSavePasswordsSwitch.setOrder(ORDER_SWITCH);
        mSavePasswordsSwitch.setSummaryOn(R.string.text_on);
        mSavePasswordsSwitch.setSummaryOff(R.string.text_off);
        mSavePasswordsSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PrefServiceBridge.getInstance().setRememberPasswordsEnabled((boolean) newValue);
                return true;
            }
        });
        mSavePasswordsSwitch.setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return PrefServiceBridge.getInstance().isRememberPasswordsManaged();
            }
        });
        getPreferenceScreen().addPreference(mSavePasswordsSwitch);

        // Note: setting the switch state before the preference is added to the screen results in
        // some odd behavior where the switch state doesn't always match the internal enabled state
        // (e.g. the switch will say "On" when save passwords is really turned off), so
        // .setChecked() should be called after .addPreference()
        mSavePasswordsSwitch.setChecked(
                PrefServiceBridge.getInstance().isRememberPasswordsEnabled());
    }

    private void createAutoSignInCheckbox() {
        mAutoSignInSwitch = new ChromeBaseCheckBoxPreference(getActivity(), null);
        mAutoSignInSwitch.setKey(PREF_AUTOSIGNIN_SWITCH);
        mAutoSignInSwitch.setTitle(R.string.passwords_auto_signin_title);
        mAutoSignInSwitch.setOrder(ORDER_AUTO_SIGNIN_CHECKBOX);
        mAutoSignInSwitch.setSummary(R.string.passwords_auto_signin_description);
        mAutoSignInSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PrefServiceBridge.getInstance().setPasswordManagerAutoSigninEnabled(
                        (boolean) newValue);
                return true;
            }
        });
        mAutoSignInSwitch.setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return PrefServiceBridge.getInstance().isPasswordManagerAutoSigninManaged();
            }
        });
        getPreferenceScreen().addPreference(mAutoSignInSwitch);
        mAutoSignInSwitch.setChecked(
                PrefServiceBridge.getInstance().isPasswordManagerAutoSigninEnabled());
    }

    private void displayManageAccountLink() {
        if (getPreferenceScreen().findPreference(PREF_KEY_MANAGE_ACCOUNT_LINK) == null) {
            if (mLinkPref == null) {
                ForegroundColorSpan colorSpan = new ForegroundColorSpan(
                        ApiCompatibilityUtils.getColor(getResources(), R.color.google_blue_700));
                SpannableString title = SpanApplier.applySpans(
                        getString(R.string.manage_passwords_text),
                        new SpanApplier.SpanInfo("<link>", "</link>", colorSpan));
                mLinkPref = new ChromeBasePreference(getActivity());
                mLinkPref.setKey(PREF_KEY_MANAGE_ACCOUNT_LINK);
                mLinkPref.setTitle(title);
                mLinkPref.setOnPreferenceClickListener(this);
                mLinkPref.setOrder(ORDER_MANAGE_ACCOUNT_LINK);
            }
            getPreferenceScreen().addPreference(mLinkPref);
        }
    }
}
