// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceCategory;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.PasswordUIView;
import org.chromium.chrome.browser.PasswordUIView.PasswordListObserver;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegate;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.ui.text.SpanApplier;

/**
 * "Manage Saved Passwords" activity, providing options to delete or
 * show saved passwords.
 */
public class ManageSavedPasswordsPreferences extends PreferenceFragment
        implements OnPreferenceChangeListener, PasswordListObserver,
        Preference.OnPreferenceClickListener {

    // Keys for name/password dictionaries.
    public static final String PASSWORD_LIST_URL = "url";
    public static final String PASSWORD_LIST_NAME = "name";

    // Used to pass the password id into a new activity.
    public static final String PASSWORD_LIST_ID = "id";

    public static final String DELETED_ITEM_IS_EXCEPTION = "is_exception";
    public static final String PASSWORD_LIST_DELETED_ID = "deleted_id";

    public static final String PREF_SAVE_PASSWORDS_SWITCH = "save_passwords_switch";

    private static final String PREF_CATEGORY_SAVED_PASSWORDS = "saved_passwords";
    private static final String PREF_CATEGORY_EXCEPTIONS = "exceptions";

    public static final int RESULT_DELETE_PASSWORD = 1;

    private final PasswordUIView mPasswordManagerHandler = new PasswordUIView();
    private TextView mEmptyView;
    private boolean mNoPasswords;
    private boolean mNoPasswordExceptions;
    private Preference mLinkPref;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.prefs_saved_passwords);
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getActivity()));
        mPasswordManagerHandler.addObserver(this);

        mEmptyView = new TextView(getActivity(), null);
        mEmptyView.setText(R.string.saved_passwords_none_text);
        mEmptyView.setGravity(Gravity.CENTER);
        mEmptyView.setVisibility(View.GONE);
        ((ViewGroup) getActivity().findViewById(android.R.id.content)).addView(mEmptyView);
    }

    /**
     * Empty screen message when no passwords or exceptions are stored.
     */
    private void displayEmptyScreenMessage() {
        if (mEmptyView != null) {
            mEmptyView.setVisibility(View.VISIBLE);
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        rebuildPasswordLists();
        return true;
    }

    void rebuildPasswordLists() {
        mNoPasswords = false;
        mNoPasswordExceptions = false;
        getPreferenceScreen().removeAll();
        mEmptyView.setVisibility(View.GONE);
        createSavePasswordsSwitch(PrefServiceBridge.getInstance().isRememberPasswordsEnabled());
        mPasswordManagerHandler.updatePasswordLists();
    }

    @Override
    public void passwordListAvailable(int count) {
        mNoPasswords = count == 0;
        if (mNoPasswords) {
            if (mNoPasswordExceptions) displayEmptyScreenMessage();
            return;
        }

        if (PasswordUIView.shouldDisplayManageAccountLink()) {
            ForegroundColorSpan colorSpan = new ForegroundColorSpan(
                    getResources().getColor(R.color.pref_accent_color));
            SpannableString title = SpanApplier.applySpans(
                    getString(R.string.manage_passwords_text),
                    new SpanApplier.SpanInfo("<link>", "</link>", colorSpan));
            mLinkPref = new ChromeBasePreference(getActivity());
            mLinkPref.setTitle(title);
            mLinkPref.setOnPreferenceClickListener(this);
            getPreferenceScreen().addPreference(mLinkPref);
        }

        PreferenceCategory profileCategory = new PreferenceCategory(getActivity());
        profileCategory.setKey(PREF_CATEGORY_SAVED_PASSWORDS);
        profileCategory.setTitle(R.string.section_saved_passwords);
        getPreferenceScreen().addPreference(profileCategory);
        for (int i = 0; i < count; i++) {
            PasswordUIView.SavedPasswordEntry saved =
                    mPasswordManagerHandler.getSavedPasswordEntry(i);
            PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getActivity());
            String url = saved.getUrl();
            String name = saved.getUserName();
            screen.setTitle(url);
            screen.setOnPreferenceClickListener(this);
            screen.setSummary(name);
            Bundle args = screen.getExtras();
            args.putString(PASSWORD_LIST_NAME, name);
            args.putString(PASSWORD_LIST_URL, url);
            args.putInt(PASSWORD_LIST_ID, i);
            profileCategory.addPreference(screen);
        }
    }

    @Override
    public void passwordExceptionListAvailable(int count) {
        mNoPasswordExceptions = count == 0;
        if (mNoPasswordExceptions) {
            if (mNoPasswords) displayEmptyScreenMessage();
            return;
        }

        PreferenceCategory profileCategory = new PreferenceCategory(getActivity());
        profileCategory.setKey(PREF_CATEGORY_EXCEPTIONS);
        profileCategory.setTitle(R.string.section_saved_passwords_exceptions);
        getPreferenceScreen().addPreference(profileCategory);
        for (int i = 0; i < count; i++) {
            String exception = mPasswordManagerHandler.getSavedPasswordException(i);
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
        rebuildPasswordLists();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mPasswordManagerHandler.destroy();
    }

    /**
     *  Preference was clicked. Either navigate to manage account site or launch the PasswordEditor
     *  depending on which preference it was.
     */
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference == mLinkPref) {
            Intent intent = new Intent(
                    Intent.ACTION_VIEW,
                    Uri.parse(PasswordUIView.getAccountDashboardURL()));
            intent.setPackage(getActivity().getPackageName());
            getActivity().startActivity(intent);
        } else {
            // Launch preference activity with PasswordEntryEditor fragment with
            // intent extras specifying the object.
            Intent intent = new Intent();
            intent.setClassName(getActivity(), getActivity().getClass().getName());
            intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT,
                    PasswordEntryEditor.class.getName());
            intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS,
                    preference.getExtras());
            startActivityForResult(intent, RESULT_DELETE_PASSWORD);
        }
        return true;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == RESULT_DELETE_PASSWORD && data != null) {
            deletePassword(data);
        }
    }

    /**
     * Called when a Password is deleted from PasswordEntryEditor.
     * @param data Intent with extras containing the index of deleted password.
     */
    private void deletePassword(Intent data) {
        if (data != null && data.hasExtra(PASSWORD_LIST_DELETED_ID)) {
            int deletedId = data.getIntExtra(PASSWORD_LIST_DELETED_ID, -1);
            boolean isException = data.getBooleanExtra(DELETED_ITEM_IS_EXCEPTION, false);
            if (isException) {
                mPasswordManagerHandler.removeSavedPasswordException(deletedId);
            } else {
                mPasswordManagerHandler.removeSavedPasswordEntry(deletedId);
            }
        }
    }

    private void createSavePasswordsSwitch(boolean isEnabled) {
        ChromeSwitchPreference savePasswordsSwitch =
                new ChromeSwitchPreference(getActivity(), null);
        savePasswordsSwitch.setKey(PREF_SAVE_PASSWORDS_SWITCH);
        savePasswordsSwitch.setSummaryOn(R.string.text_on);
        savePasswordsSwitch.setSummaryOff(R.string.text_off);
        savePasswordsSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PrefServiceBridge.getInstance().setRememberPasswordsEnabled((boolean) newValue);
                return true;
            }
        });

        savePasswordsSwitch.setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return PrefServiceBridge.getInstance().isRememberPasswordsManaged();
            }
        });

        getPreferenceScreen().addPreference(savePasswordsSwitch);

        // Note: setting the switch state before the preference is added to the screen results in
        // some odd behavior where the switch state doesn't always match the internal enabled state
        // (e.g. the switch will say "On" when save passwords is really turned off), so
        // .setChecked() should be called after .addPreference()
        savePasswordsSwitch.setChecked(isEnabled);
    }
}
