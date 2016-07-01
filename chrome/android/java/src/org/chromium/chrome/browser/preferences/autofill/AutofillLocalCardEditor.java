// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.widget.CompatibilityTextInputLayout;

import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Locale;

/**
 * Local credit card settings.
 */
public class AutofillLocalCardEditor extends AutofillCreditCardEditor {
    private CompatibilityTextInputLayout mNameLabel;
    private EditText mNameText;
    private CompatibilityTextInputLayout mNumberLabel;
    private EditText mNumberText;
    private Spinner mExpirationMonth;
    private Spinner mExpirationYear;

    private int mInitialExpirationMonthPos;
    private int mInitialExpirationYearPos;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View v = super.onCreateView(inflater, container, savedInstanceState);

        mNameLabel = (CompatibilityTextInputLayout) v.findViewById(R.id.credit_card_name_label);
        mNameText = (EditText) v.findViewById(R.id.credit_card_name_edit);
        mNumberLabel = (CompatibilityTextInputLayout) v.findViewById(R.id.credit_card_number_label);
        mNumberText = (EditText) v.findViewById(R.id.credit_card_number_edit);

        // Set text watcher to format credit card number
        mNumberText.addTextChangedListener(new CreditCardNumberFormattingTextWatcher());

        mExpirationMonth = (Spinner) v.findViewById(R.id.autofill_credit_card_editor_month_spinner);
        mExpirationYear = (Spinner) v.findViewById(R.id.autofill_credit_card_editor_year_spinner);

        addSpinnerAdapters();
        addCardDataToEditFields();
        initializeButtons(v);
        return v;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.autofill_local_card_editor;
    }

    @Override
    protected int getTitleResourceId(boolean isNewEntry) {
        return isNewEntry
                ? R.string.autofill_create_credit_card : R.string.autofill_edit_credit_card;
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if ((parent == mExpirationYear && position != mInitialExpirationYearPos)
                || (parent == mExpirationMonth && position != mInitialExpirationMonthPos)
                || (parent == mBillingAddress && position != mInitialBillingAddressPos)) {
            updateSaveButtonEnabled();
        }
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        updateSaveButtonEnabled();
    }

    void addSpinnerAdapters() {
        ArrayAdapter<CharSequence> adapter = new ArrayAdapter<CharSequence>(getActivity(),
                android.R.layout.simple_spinner_item);

        // Populate the month dropdown.
        Calendar calendar = Calendar.getInstance();
        calendar.set(Calendar.DAY_OF_MONTH, 1);
        SimpleDateFormat formatter = new SimpleDateFormat("MMMM (MM)", Locale.getDefault());

        for (int month = 0; month < 12; month++) {
            calendar.set(Calendar.MONTH, month);
            adapter.add(formatter.format(calendar.getTime()));
        }
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mExpirationMonth.setAdapter(adapter);

        // Populate the year dropdown.
        adapter = new ArrayAdapter<CharSequence>(getActivity(),
                android.R.layout.simple_spinner_item);
        int initialYear = calendar.get(Calendar.YEAR);
        for (int year = initialYear; year < initialYear + 10; year++) {
            adapter.add(Integer.toString(year));
        }
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mExpirationYear.setAdapter(adapter);
    }

    private void addCardDataToEditFields() {
        if (mCard == null) {
            mNameLabel.requestFocus();
            return;
        }

        if (!TextUtils.isEmpty(mCard.getName())) {
            mNameLabel.getEditText().setText(mCard.getName());
        }
        if (!TextUtils.isEmpty(mCard.getNumber())) {
            mNumberLabel.getEditText().setText(mCard.getNumber());
        }

        // Make the name label focusable in touch mode so that mNameText doesn't get focused.
        mNameLabel.setFocusableInTouchMode(true);

        int monthAsInt = 1;
        if (!mCard.getMonth().isEmpty()) {
            monthAsInt = Integer.parseInt(mCard.getMonth());
        }
        mInitialExpirationMonthPos = monthAsInt - 1;
        mExpirationMonth.setSelection(mInitialExpirationMonthPos);

        mInitialExpirationYearPos = 0;
        boolean foundYear = false;
        for (int i = 0; i < mExpirationYear.getAdapter().getCount(); i++) {
            if (mCard.getYear().equals(mExpirationYear.getAdapter().getItem(i))) {
                mInitialExpirationYearPos = i;
                foundYear = true;
                break;
            }
        }
        // Maybe your card expired years ago? Add the card's year
        // to the spinner adapter if not found.
        if (!foundYear && !mCard.getYear().isEmpty()) {
            @SuppressWarnings("unchecked")
            ArrayAdapter<CharSequence> adapter =
                    (ArrayAdapter<CharSequence>) mExpirationYear.getAdapter();
            adapter.insert(mCard.getYear(), 0);
            mInitialExpirationYearPos = 0;
        }
        mExpirationYear.setSelection(mInitialExpirationYearPos);
    }

    @Override
    protected void saveEntry() {
        // Remove all spaces in editText.
        String cardNumber = mNumberText.getText().toString().replaceAll("\\s+", "");
        CreditCard card = new CreditCard(mGUID, AutofillPreferences.SETTINGS_ORIGIN,
                true /* isLocal */, false /* isCached */, mNameText.getText().toString().trim(),
                cardNumber, "" /* obfuscatedNumber */,
                String.valueOf(mExpirationMonth.getSelectedItemPosition() + 1),
                (String) mExpirationYear.getSelectedItem(), "" /* basicCardPaymentType */,
                0 /* issuerIconDrawableId */,
                ((AutofillProfile) mBillingAddress.getSelectedItem()).getGUID() /* billing */);
        PersonalDataManager.getInstance().setCreditCard(card);
    }

    @Override
    protected void deleteEntry() {
        if (mGUID != null) {
            PersonalDataManager.getInstance().deleteCreditCard(mGUID);
        }
    }

    @Override
    protected void initializeButtons(View v) {
        super.initializeButtons(v);

        // Listen for changes to inputs. Enable the save button after something has changed.
        mNameText.addTextChangedListener(this);
        mNumberText.addTextChangedListener(this);
        mExpirationMonth.setOnItemSelectedListener(this);
        mExpirationYear.setOnItemSelectedListener(this);
        mBillingAddress.setOnItemSelectedListener(this);
    }

    private void updateSaveButtonEnabled() {
        boolean enabled = !TextUtils.isEmpty(mNameText.getText())
                || !TextUtils.isEmpty(mNumberText.getText());
        ((Button) getView().findViewById(R.id.button_primary)).setEnabled(enabled);
    }

    /**
     * Watch a TextView and if a credit card number is entered, it will format the number.
     * Disable formatting when user:
     * 1. Inputs dashes or spaces.
     * 2. Removes separators in the middle of the string
     * 3. Enters a number longer than 16 digits.
     *
     * Formatting will be re-enabled once text is cleared.
     */
    private static class CreditCardNumberFormattingTextWatcher implements TextWatcher {
        /** Character for card number section separator. */
        private static final String SEPARATOR = " ";

        /**
         * Whether to format the credit card number. If true, spaces will be inserted
         * automatically between each group of 4 digits in the credit card number as the user types.
         * This is set to false if the user types a dash or deletes one of the auto-inserted spaces.
         */
        private boolean mFormattingEnabled = true;

        /**
         * Whether the change was caused by ourselves.
         * This is set true when we are manipulating the text of EditText,
         * and all callback functions should check this boolean to avoid infinite recursion.
         */
        private boolean mSelfChange = false;

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            if (mSelfChange || !mFormattingEnabled) return;
            // If user enters non-digit characters, do not format.
            if (count > 0 && hasDashOrSpace(s, start, count)) {
                mFormattingEnabled = false;
            }
        }

        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            if (mSelfChange || !mFormattingEnabled) return;
            // If user deletes non-digit characters, do not format.
            if (count > 0 && hasDashOrSpace(s, start, count)) {
                mFormattingEnabled = false;
            }
        }

        @Override
        public void afterTextChanged(Editable s) {
            if (mSelfChange) return;
            mSelfChange = true;

            if (mFormattingEnabled) {
                removeSeparators(s);
                // If number is too long, do not format it and remove all
                // previous separators.
                if (s.length() > 16) {
                    mFormattingEnabled = false;
                } else {
                    insertSeparators(s);
                }
            }
            // If user clears the input, re-enable formatting
            if (s.length() == 0) mFormattingEnabled = true;

            mSelfChange = false;
        }

        public static void removeSeparators(Editable s) {
            int index = TextUtils.indexOf(s, SEPARATOR);
            while (index >= 0) {
                s.delete(index, index + 1);
                index = TextUtils.indexOf(s, SEPARATOR, index + 1);
            }
        }

        public static void insertSeparators(Editable s) {
            final int[] positions = {4, 9, 14 };
            for (int i : positions) {
                if (s.length() > i) {
                    s.insert(i, SEPARATOR);
                }
            }
        }

        public static boolean hasDashOrSpace(final CharSequence s, final int start,
                final int count) {
            return TextUtils.indexOf(s, " ", start, start + count) != -1
                    || TextUtils.indexOf(s, "-", start, start + count) != -1;
        }
    }
}
