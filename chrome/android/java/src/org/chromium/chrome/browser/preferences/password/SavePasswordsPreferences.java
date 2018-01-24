// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.content.ActivityNotFoundException;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceCategory;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.support.annotation.Nullable;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
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

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;

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

    // The key for saving |mExportFileUri| to instance bundle.
    private static final String SAVED_STATE_EXPORT_FILE_URI = "saved-state-export-file-uri";

    public static final String PREF_SAVE_PASSWORDS_SWITCH = "save_passwords_switch";
    public static final String PREF_AUTOSIGNIN_SWITCH = "autosignin_switch";

    private static final String PREF_KEY_CATEGORY_SAVED_PASSWORDS = "saved_passwords";
    private static final String PREF_KEY_CATEGORY_EXCEPTIONS = "exceptions";
    private static final String PREF_KEY_MANAGE_ACCOUNT_LINK = "manage_account_link";
    private static final String PREF_KEY_SAVED_PASSWORDS_NO_TEXT = "saved_passwords_no_text";

    // Name of the feature controlling the password export functionality.
    private static final String EXPORT_PASSWORDS = "PasswordExport";

    // Name of the subdirectory in cache which stores the exported passwords file.
    private static final String PASSWORDS_CACHE_DIR = "/passwords";

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
    // True if the option to export passwords in the three-dots menu should be disabled due to an
    // ongoing export.
    private boolean mExportOptionSuspended = false;
    // True if the user just finished the UI flow for confirming a password export.
    private boolean mExportConfirmed;
    // When the user requests that passwords are exported and once the passwords are sent over from
    // native code and stored in a cache file, this variable contains the content:// URI for that
    // cache file, or an empty URI if there was a problem with storing to that file. During all
    // other times, this variable is null. In particular, after the export is requested, the
    // variable being null means that the passwords have not arrived from the native code yet.
    @Nullable
    private Uri mExportFileUri;

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

        if (savedInstanceState == null) return;

        if (savedInstanceState.containsKey(SAVED_STATE_EXPORT_REQUESTED)) {
            mExportRequested =
                    savedInstanceState.getBoolean(SAVED_STATE_EXPORT_REQUESTED, mExportRequested);
        }
        if (savedInstanceState.containsKey(SAVED_STATE_EXPORT_FILE_URI)) {
            String uriString = savedInstanceState.getString(SAVED_STATE_EXPORT_FILE_URI);
            if (uriString.isEmpty()) {
                mExportFileUri = Uri.EMPTY;
            } else {
                mExportFileUri = Uri.parse(uriString);
            }
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
        menu.findItem(R.id.export_passwords).setEnabled(!mNoPasswords && !mExportOptionSuspended);
        super.onPrepareOptionsMenu(menu);
    }

    // An encapsulation of a URI and an error string, used by the processing in
    // exportPasswordsIntoFile.
    private static class ExportResult {
        public final Uri mUri;
        @Nullable
        public final String mError;

        // Constructs the successful result: a valid URI and no error.
        public ExportResult(Uri uri) {
            assert uri != null && uri != Uri.EMPTY;
            mUri = uri;
            mError = null;
        }

        // Constructs the failed result: an empty URI and a non-empty error string.
        public ExportResult(String error) {
            assert !TextUtils.isEmpty(error);
            mUri = Uri.EMPTY;
            mError = error;
        }
    }

    /**
     * A helper method which first fires an AsyncTask to turn the string with serialized passwords
     * into a cache file with a shareable URI, and then, depending on success, either calls the code
     * for firing the share intent or displays an error.
     * @param serializedPasswords A string with a CSV representation of the user's passwords.
     */
    private void shareSerializedPasswords(String serializedPasswords) {
        AsyncTask<String, Void, ExportResult> task = new AsyncTask<String, Void, ExportResult>() {
            @Override
            protected ExportResult doInBackground(String... serializedPasswords) {
                assert serializedPasswords.length == 1;
                return exportPasswordsIntoFile(serializedPasswords[0]);
            }

            @Override
            protected void onPostExecute(ExportResult result) {
                if (result.mError != null) {
                    showExportErrorAndAbort(result.mError);
                } else {
                    mExportFileUri = result.mUri;
                    tryExporting();
                }
            }
        };
        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, serializedPasswords);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.export_passwords) {
            // Disable re-triggering exporting until the current exporting finishes.
            mExportOptionSuspended = true;

            // Start fetching the serialized passwords now to use the time the user spends
            // reauthenticating and reading the warning message. If the user cancels the export or
            // fails the reauthentication, the serialised passwords will simply get ignored when
            // they arrive.
            PasswordManagerHandlerProvider.getInstance()
                    .getPasswordManagerHandler()
                    .serializePasswords(new Callback<String>() {
                        @Override
                        public void onResult(String serializedPasswords) {
                            shareSerializedPasswords(serializedPasswords);
                        }
                    });
            if (!ReauthenticationManager.isScreenLockSetUp(getActivity().getApplicationContext())) {
                Toast.makeText(getActivity().getApplicationContext(),
                             R.string.password_export_set_lock_screen, Toast.LENGTH_LONG)
                        .show();
                // Re-enable exporting, the current one was cancelled by Chrome.
                mExportOptionSuspended = false;
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
                    mExportConfirmed = true;
                    tryExporting();
                }
                // Re-enable exporting, the current one was either finished or dismissed.
                mExportOptionSuspended = false;
            }
        });
        exportWarningDialogFragment.show(getFragmentManager(), null);
    }

    /**
     * Starts the exporting intent if both blocking events are completed: serializing and the
     * confirmation flow.
     */
    private void tryExporting() {
        // TODO(crbug.com/788701): Display a progress indicator if user
        // confirmed but serialising is not done yet and dismiss it once called
        // again with serialising done.
        if (mExportConfirmed && mExportFileUri != null) sendExportIntent();
    }

    /**
     * Call this to abort the export UI flow and display an error description to the user.
     * @param description A string with a brief explanation of the error.
     */
    private void showExportErrorAndAbort(String description) {
        // TODO(crbug.com/788701): Implement.
        // Re-enable exporting, the current one was just cancelled.
        mExportOptionSuspended = false;
    }

    /**
     * This method saves the contents of |serializedPasswords| into a temporary file and returns a
     * sharing URI for it. In case of failure, returns EMPTY. It should only be run on the
     * background thread of an AsyncTask, because it does I/O operations.
     * @param serializedPasswords A string with serialized passwords in CSV format
     */
    private ExportResult exportPasswordsIntoFile(String serializedPasswords) {
        // First ensure that the PASSWORDS_CACHE_DIR cache directory exists.
        File passwordsDir =
                new File(ContextUtils.getApplicationContext().getCacheDir() + PASSWORDS_CACHE_DIR);
        passwordsDir.mkdir();
        // Now create or overwrite the temporary file for exported passwords there and return its
        // content:// URI.
        File tempFile;
        try {
            tempFile = File.createTempFile("pwd-export", ".csv", passwordsDir);
        } catch (IOException e) {
            // TODO(crbug.com/788701): Change e.getMessage to an appropriate error, following the
            // mocks.
            return new ExportResult(e.getMessage());
        }
        tempFile.deleteOnExit();
        try (BufferedWriter tempWriter = new BufferedWriter(new OutputStreamWriter(
                     new FileOutputStream(tempFile), Charset.forName("UTF-8")))) {
            tempWriter.write(serializedPasswords);
        } catch (IOException e) {
            // TODO(crbug.com/788701): Change e.getMessage to an appropriate error, following the
            // mocks.
            return new ExportResult(e.getMessage());
        }
        try {
            return new ExportResult(ContentUriUtils.getContentUriFromFile(tempFile));
        } catch (IllegalArgumentException e) {
            // TODO(crbug.com/788701): Display an error, because the result of Uri.fromFile is not
            // going to be shareable.
            return new ExportResult(e.getMessage());
        }
    }

    /**
     * If the URI of the file with exported passwords is not null, passes it into an implicit
     * intent, so that the user can use a storage app to save the exported passwords.
     */
    private void sendExportIntent() {
        mExportConfirmed = false;
        if (mExportFileUri == Uri.EMPTY) return;

        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/csv");
        send.putExtra(Intent.EXTRA_STREAM, mExportFileUri);

        try {
            Intent chooser = Intent.createChooser(send, null);
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ContextUtils.getApplicationContext().startActivity(chooser);
        } catch (ActivityNotFoundException e) {
            // TODO(crbug.com/788701): If no app handles it, display the appropriate error.
            showExportErrorAndAbort(e.getMessage());
        }
        mExportFileUri = null;
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
            // Depending on the authentication result, either carry on with exporting or re-enable
            // the export menu for future attempts.
            if (ReauthenticationManager.authenticationStillValid()) {
                exportAfterReauth();
            } else {
                mExportOptionSuspended = false;
            }
        }
        rebuildPasswordLists();
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(SAVED_STATE_EXPORT_REQUESTED, mExportRequested);
        if (mExportFileUri != null) {
            outState.putString(SAVED_STATE_EXPORT_FILE_URI, mExportFileUri.toString());
        }
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
