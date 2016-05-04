// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.app.Activity;
import android.app.ProgressDialog;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.widget.ListView;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BrowsingDataType;
import org.chromium.chrome.browser.TimePeriod;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.preferences.ButtonPreference;
import org.chromium.chrome.browser.preferences.ClearBrowsingDataCheckBoxPreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.SpinnerPreference;
import org.chromium.chrome.browser.preferences.TextMessageWithLinkAndIconPreference;
import org.chromium.chrome.browser.preferences.privacy.BrowsingDataCounterBridge.BrowsingDataCounterCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.sync.signin.ChromeSigninController;

import java.util.Arrays;
import java.util.EnumSet;

/**
 * Preference screen that allows the user to clear browsing data.
 * The user can choose which types of data to clear (history, cookies, etc), and the time range
 * from which to clear data.
 */
public class ClearBrowsingDataPreferences extends PreferenceFragment
        implements PrefServiceBridge.OnClearBrowsingDataListener,
                   PrefServiceBridge.OtherFormsOfBrowsingHistoryListener,
                   Preference.OnPreferenceClickListener,
                   Preference.OnPreferenceChangeListener {
    /**
     * Represents a single item in the dialog.
     */
    private static class Item implements BrowsingDataCounterCallback,
                                         Preference.OnPreferenceClickListener {
        private final ClearBrowsingDataPreferences mParent;
        private final DialogOption mOption;
        private final ClearBrowsingDataCheckBoxPreference mCheckbox;
        private BrowsingDataCounterBridge mCounter;
        private boolean mShouldAnnounceCounterResult;

        public Item(ClearBrowsingDataPreferences parent,
                    DialogOption option,
                    ClearBrowsingDataCheckBoxPreference checkbox,
                    boolean selected,
                    boolean enabled) {
            super();
            mParent = parent;
            mOption = option;
            mCheckbox = checkbox;
            mCounter = new BrowsingDataCounterBridge(this, mOption.getDataType());

            mCheckbox.setOnPreferenceClickListener(this);
            mCheckbox.setEnabled(enabled);
            mCheckbox.setChecked(selected);
            mCheckbox.setSummaryOff("");  // No summary when unchecked.
        }

        public void destroy() {
            mCounter.destroy();
        }

        public DialogOption getOption() {
            return mOption;
        }

        public boolean isSelected() {
            return mCheckbox.isChecked();
        }

        @Override
        public boolean onPreferenceClick(Preference preference) {
            assert mCheckbox == preference;

            mParent.updateButtonState();
            mShouldAnnounceCounterResult = true;
            PrefServiceBridge.getInstance().setBrowsingDataDeletionPreference(
                    mOption.getDataType(), mCheckbox.isChecked());
            return true;
        }

        @Override
        public void onCounterFinished(String result) {
            mCheckbox.setSummaryOn(result);
            if (mShouldAnnounceCounterResult) {
                mCheckbox.announceForAccessibility(result);
            }
        }

        /**
         * Sets whether the BrowsingDataCounter result should be announced. This is when the counter
         * recalculation was caused by a checkbox state change (as opposed to fragment
         * initialization or time period change).
         */
        public void setShouldAnnounceCounterResult(boolean value) {
            mShouldAnnounceCounterResult = value;
        }
    }

    private static final String PREF_HISTORY = "clear_history_checkbox";
    private static final String PREF_COOKIES = "clear_cookies_checkbox";
    private static final String PREF_CACHE = "clear_cache_checkbox";
    private static final String PREF_PASSWORDS = "clear_passwords_checkbox";
    private static final String PREF_FORM_DATA = "clear_form_data_checkbox";

    @VisibleForTesting
    public static final String PREF_GOOGLE_SUMMARY = "google_summary";
    @VisibleForTesting
    public static final String PREF_GENERAL_SUMMARY = "general_summary";
    private static final String PREF_TIME_RANGE = "time_period_spinner";

    /** The "Clear" button preference. */
    @VisibleForTesting
    public static final String PREF_CLEAR_BUTTON = "clear_button";

    /** The tag used for logging. */
    public static final String TAG = "ClearBrowsingDataPreferences";

    /** The histogram for the dialog about other forms of browsing history. */
    private static final String DIALOG_HISTOGRAM =
            "History.ClearBrowsingData.ShownHistoryNoticeAfterClearing";

    /** The web history URL. */
    private static final String WEB_HISTORY_URL =
            "https://history.google.com/history/?utm_source=chrome_cbd";

    /**
     * The various data types that can be cleared via this screen.
     */
    public enum DialogOption {
        CLEAR_HISTORY(BrowsingDataType.HISTORY, PREF_HISTORY),
        CLEAR_COOKIES_AND_SITE_DATA(BrowsingDataType.COOKIES, PREF_COOKIES),
        CLEAR_CACHE(BrowsingDataType.CACHE, PREF_CACHE),
        CLEAR_PASSWORDS(BrowsingDataType.PASSWORDS, PREF_PASSWORDS),
        CLEAR_FORM_DATA(BrowsingDataType.FORM_DATA, PREF_FORM_DATA);

        private final int mDataType;
        private final String mPreferenceKey;

        private DialogOption(int dataType, String preferenceKey) {
            mDataType = dataType;
            mPreferenceKey = preferenceKey;
        }

        public int getDataType() {
            return mDataType;
        }

        /**
         * @return String The key of the corresponding preference.
         */
        public String getPreferenceKey() {
            return mPreferenceKey;
        }
    }

    /**
     * An option to be shown in the time period spiner.
     */
    private static class TimePeriodSpinnerOption {
        private int mTimePeriod;
        private String mTitle;

        /**
         * Constructs this time period spinner option.
         * @param timePeriod The time period represented as an int from the shared enum
         *     {@link TimePeriod}.
         * @param title The text that will be used to represent this item in the spinner.
         */
        public TimePeriodSpinnerOption(int timePeriod, String title) {
            mTimePeriod = timePeriod;
            mTitle = title;
        }

        /**
         * @return The time period represented as an int from the shared enum {@link TimePeriod}
         */
        public int getTimePeriod() {
            return mTimePeriod;
        }

        @Override
        public String toString() {
            return mTitle;
        }
    }

    private OtherFormsOfHistoryDialogFragment mDialogAboutOtherFormsOfBrowsingHistory;
    private boolean mIsDialogAboutOtherFormsOfBrowsingHistoryEnabled;

    private ProgressDialog mProgressDialog;
    private Item[] mItems;

    private final EnumSet<DialogOption> getSelectedOptions() {
        EnumSet<DialogOption> selected = EnumSet.noneOf(DialogOption.class);
        for (Item item : mItems) {
            if (item.isSelected()) selected.add(item.getOption());
        }
        return selected;
    }

    /**
     * Requests the browsing data corresponding to the given dialog options to be deleted.
     * @param options The dialog options whose corresponding data should be deleted.
     */
    private final void clearBrowsingData(EnumSet<DialogOption> options) {
        showProgressDialog();

        int[] dataTypes = new int[options.size()];
        int i = 0;
        for (DialogOption option : options) {
            dataTypes[i] = option.getDataType();
            ++i;
        }

        Object spinnerSelection =
                ((SpinnerPreference) findPreference(PREF_TIME_RANGE)).getSelectedOption();
        int timePeriod = ((TimePeriodSpinnerOption) spinnerSelection).getTimePeriod();
        PrefServiceBridge.getInstance().clearBrowsingData(this, dataTypes, timePeriod);
    }

    private void dismissProgressDialog() {
        if (mProgressDialog != null && mProgressDialog.isShowing()) {
            mProgressDialog.dismiss();
        }
        mProgressDialog = null;
    }

    /**
     * Returns the Array of dialog options. Options are displayed in the same
     * order as they appear in the array.
     */
    private DialogOption[] getDialogOptions() {
        return new DialogOption[] {
            DialogOption.CLEAR_HISTORY,
            DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
            DialogOption.CLEAR_CACHE,
            DialogOption.CLEAR_PASSWORDS,
            DialogOption.CLEAR_FORM_DATA};
    }

    /**
     * Returns the Array of time periods. Options are displayed in the same order as they appear
     * in the array.
     */
    private TimePeriodSpinnerOption[] getTimePeriodSpinnerOptions() {
        Activity activity = getActivity();

        TimePeriodSpinnerOption[] options = new TimePeriodSpinnerOption[] {
                new TimePeriodSpinnerOption(TimePeriod.LAST_HOUR,
                        activity.getString(R.string.clear_browsing_data_period_hour)),
                new TimePeriodSpinnerOption(TimePeriod.LAST_DAY,
                        activity.getString(R.string.clear_browsing_data_period_day)),
                new TimePeriodSpinnerOption(TimePeriod.LAST_WEEK,
                        activity.getString(R.string.clear_browsing_data_period_week)),
                new TimePeriodSpinnerOption(TimePeriod.FOUR_WEEKS,
                        activity.getString(R.string.clear_browsing_data_period_four_weeks)),
                new TimePeriodSpinnerOption(TimePeriod.EVERYTHING,
                        activity.getString(R.string.clear_browsing_data_period_everything))};

        return options;
    }

    /**
     * Decides whether a given dialog option should be selected when the dialog is initialized.
     * @param option The option in question.
     * @return boolean Whether the given option should be preselected.
     */
    private boolean isOptionSelectedByDefault(DialogOption option) {
        return PrefServiceBridge.getInstance().getBrowsingDataDeletionPreference(
            option.getDataType());
    }

    /**
     * Called when clearing browsing data completes.
     * Implements the ChromePreferences.OnClearBrowsingDataListener interface.
     */
    @Override
    public void onBrowsingDataCleared() {
        if (getActivity() == null) return;

        // If the user deleted their browsing history, the dialog about other forms of history
        // is enabled, and it has never been shown before, show it. Otherwise, just close this
        // preference screen.
        if (getSelectedOptions().contains(DialogOption.CLEAR_HISTORY)
                && mIsDialogAboutOtherFormsOfBrowsingHistoryEnabled
                && !OtherFormsOfHistoryDialogFragment.wasDialogShown(getActivity())) {
            mDialogAboutOtherFormsOfBrowsingHistory =
                    OtherFormsOfHistoryDialogFragment.show(getActivity());
            dismissProgressDialog();
            RecordHistogram.recordBooleanHistogram(DIALOG_HISTOGRAM, true);
        } else {
            dismissProgressDialog();
            getActivity().finish();
            RecordHistogram.recordBooleanHistogram(DIALOG_HISTOGRAM, false);
        }
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference.getKey().equals(PREF_CLEAR_BUTTON)) {
            clearBrowsingData(getSelectedOptions());
            return true;
        }
        return false;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object value) {
        if (preference.getKey().equals(PREF_TIME_RANGE)) {
            // Inform the items that a recalculation is going to happen as a result of the time
            // period change.
            for (Item item : mItems) {
                item.setShouldAnnounceCounterResult(false);
            }

            PrefServiceBridge.getInstance().setBrowsingDataDeletionTimePeriod(
                    ((TimePeriodSpinnerOption) value).getTimePeriod());
            return true;
        }
        return false;
    }

    /**
     * Disable the "Clear" button if none of the options are selected. Otherwise, enable it.
     */
    private void updateButtonState() {
        ButtonPreference clearButton = (ButtonPreference) findPreference(PREF_CLEAR_BUTTON);
        if (clearButton == null) return;
        boolean isEnabled = !getSelectedOptions().isEmpty();
        clearButton.setEnabled(isEnabled);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        PrefServiceBridge.getInstance().requestInfoAboutOtherFormsOfBrowsingHistory(this);
        getActivity().setTitle(R.string.clear_browsing_data_title);
        addPreferencesFromResource(R.xml.clear_browsing_data_preferences);
        DialogOption[] options = getDialogOptions();
        mItems = new Item[options.length];
        for (int i = 0; i < options.length; i++) {
            boolean enabled = true;

            // It is possible to disable the deletion of browsing history.
            if (options[i] == DialogOption.CLEAR_HISTORY
                    && !PrefServiceBridge.getInstance().canDeleteBrowsingHistory()) {
                enabled = false;
                PrefServiceBridge.getInstance().setBrowsingDataDeletionPreference(
                        DialogOption.CLEAR_HISTORY.getDataType(), false);
            }

            mItems[i] = new Item(
                this,
                options[i],
                (ClearBrowsingDataCheckBoxPreference) findPreference(options[i].getPreferenceKey()),
                isOptionSelectedByDefault(options[i]),
                enabled);
        }

        // Not all checkboxes defined in the layout are necessarily handled by this class
        // or a particular subclass. Hide those that are not.
        EnumSet<DialogOption> unboundOptions = EnumSet.allOf(DialogOption.class);
        unboundOptions.removeAll(Arrays.asList(getDialogOptions()));
        for (DialogOption option : unboundOptions) {
            getPreferenceScreen().removePreference(findPreference(option.getPreferenceKey()));
        }

        // The time range selection spinner.
        SpinnerPreference spinner = (SpinnerPreference) findPreference(PREF_TIME_RANGE);
        spinner.setOnPreferenceChangeListener(this);
        TimePeriodSpinnerOption[] spinnerOptions = getTimePeriodSpinnerOptions();
        int selectedTimePeriod =
                PrefServiceBridge.getInstance().getBrowsingDataDeletionTimePeriod();
        int spinnerOptionIndex = -1;
        for (int i = 0; i < spinnerOptions.length; ++i) {
            if (spinnerOptions[i].getTimePeriod() == selectedTimePeriod) {
                spinnerOptionIndex = i;
                break;
            }
        }
        assert spinnerOptionIndex != -1;
        spinner.setOptions(spinnerOptions, spinnerOptionIndex);

        // The "Clear" button.
        ButtonPreference clearButton = (ButtonPreference) findPreference(PREF_CLEAR_BUTTON);
        clearButton.setOnPreferenceClickListener(this);
        clearButton.setShouldDisableView(true);

        // The general information footnote informs users about data that will not be deleted.
        // If the user is signed in, it also informs users about the behavior of synced deletions.
        // and we show an additional Google-specific footnote. This footnote informs users that they
        // will not be signed out of their Google account, and if the web history service indicates
        // that they have other forms of browsing history, then also about that.
        TextMessageWithLinkAndIconPreference google_summary =
                (TextMessageWithLinkAndIconPreference) findPreference(PREF_GOOGLE_SUMMARY);
        TextMessageWithLinkAndIconPreference general_summary =
                (TextMessageWithLinkAndIconPreference) findPreference(PREF_GENERAL_SUMMARY);

        google_summary.setLinkClickDelegate(new Runnable() {
            @Override
            public void run() {
                new TabDelegate(false /* incognito */).launchUrl(
                        WEB_HISTORY_URL, TabLaunchType.FROM_CHROME_UI);
            }
        });
        general_summary.setLinkClickDelegate(new Runnable() {
            @Override
            public void run() {
                HelpAndFeedback.getInstance(getActivity()).show(
                        getActivity(),
                        getResources().getString(R.string.help_context_clear_browsing_data),
                        Profile.getLastUsedProfile(),
                        null);
            }
        });
        if (ChromeSigninController.get(getActivity()).isSignedIn()) {
            general_summary.setSummary(
                    R.string.clear_browsing_data_footnote_sync_and_site_settings);
        } else {
            getPreferenceScreen().removePreference(google_summary);
            general_summary.setSummary(R.string.clear_browsing_data_footnote_site_settings);
        }
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        // Now that the dialog's view has been created, update the button state.
        updateButtonState();

        // Remove the dividers between checkboxes.
        ((ListView) getView().findViewById(android.R.id.list)).setDivider(null);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        dismissProgressDialog();
        for (Item item : mItems) {
            item.destroy();
        }
    }

    private final void showProgressDialog() {
        if (getActivity() == null) return;

        mProgressDialog = ProgressDialog.show(getActivity(),
                getActivity().getString(R.string.clear_browsing_data_progress_title),
                getActivity().getString(R.string.clear_browsing_data_progress_message), true,
                false);
    }

    @VisibleForTesting
    ProgressDialog getProgressDialog() {
        return mProgressDialog;
    }

    @Override
    public void showNoticeAboutOtherFormsOfBrowsingHistory() {
        if (getActivity() == null) return;

        TextMessageWithLinkAndIconPreference google_summary =
                (TextMessageWithLinkAndIconPreference) findPreference(PREF_GOOGLE_SUMMARY);
        if (google_summary == null) return;
        google_summary.setSummary(
                R.string.clear_browsing_data_footnote_signed_and_other_forms_of_history);
    }

    @Override
    public void enableDialogAboutOtherFormsOfBrowsingHistory() {
        if (getActivity() == null) return;

        mIsDialogAboutOtherFormsOfBrowsingHistoryEnabled = true;
    }

    /**
     * Used only to access the dialog about other forms of browsing history from tests.
     */
    @VisibleForTesting
    OtherFormsOfHistoryDialogFragment getDialogAboutOtherFormsOfBrowsingHistory() {
        return mDialogAboutOtherFormsOfBrowsingHistory;
    }
}
