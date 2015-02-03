// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Handler;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.text.NumberFormat;
import java.util.Calendar;

/**
 * A prompt that bugs users to enter their CVC when unmasking a Wallet instrument (credit card).
 */
public class CardUnmaskPrompt implements DialogInterface.OnDismissListener, TextWatcher {
    private final CardUnmaskPromptDelegate mDelegate;
    private final AlertDialog mDialog;
    private final boolean mShouldRequestExpirationDate;

    private final EditText mCardUnmaskInput;
    private final Spinner mMonthSpinner;
    private final Spinner mYearSpinner;
    private final ProgressBar mVerificationProgressBar;
    private final TextView mVerificationView;

    /**
     * An interface to handle the interaction with an CardUnmaskPrompt object.
     */
    public interface CardUnmaskPromptDelegate {
        /**
         * Called when the dialog has been dismissed.
         */
        void dismissed();

        /**
         * Returns whether |userResponse| represents a valid value.
         */
        boolean checkUserInputValidity(String userResponse);

        /**
         * Called when the user has entered a value and pressed "verify".
         * @param userResponse The value the user entered (a CVC), or an empty string if the
         *        user canceled.
         */
        void onUserInput(String cvc, String month, String year);
    }

    public CardUnmaskPrompt(Context context, CardUnmaskPromptDelegate delegate, String title,
            String instructions, int drawableId, boolean shouldRequestExpirationDate) {
        mDelegate = delegate;

        LayoutInflater inflater = LayoutInflater.from(context);
        View v = inflater.inflate(R.layout.autofill_card_unmask_prompt, null);
        ((TextView) v.findViewById(R.id.instructions)).setText(instructions);

        mCardUnmaskInput = (EditText) v.findViewById(R.id.card_unmask_input);
        mMonthSpinner = (Spinner) v.findViewById(R.id.expiration_month);
        mYearSpinner = (Spinner) v.findViewById(R.id.expiration_year);
        mVerificationProgressBar = (ProgressBar) v.findViewById(R.id.verification_progress_bar);
        mVerificationView = (TextView) v.findViewById(R.id.verification_message);
        ((ImageView) v.findViewById(R.id.cvc_hint_image)).setImageResource(drawableId);

        mDialog = new AlertDialog.Builder(context)
                          .setTitle(title)
                          .setView(v)
                          .setNegativeButton(R.string.cancel, null)
                          .setPositiveButton(R.string.card_unmask_confirm_button, null)
                          .setOnDismissListener(this)
                          .create();

        mShouldRequestExpirationDate = shouldRequestExpirationDate;
    }

    public void show() {
        mDialog.show();

        if (mShouldRequestExpirationDate) initializeExpirationDateSpinners();

        // Override the View.OnClickListener so that pressing the positive button doesn't dismiss
        // the dialog.
        Button verifyButton = mDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        verifyButton.setEnabled(false);
        verifyButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                mDelegate.onUserInput(mCardUnmaskInput.getText().toString(),
                        (String) mMonthSpinner.getSelectedItem(),
                        (String) mYearSpinner.getSelectedItem());
            }
        });

        final EditText input = mCardUnmaskInput;
        input.addTextChangedListener(this);
        input.post(new Runnable() {
            @Override
            public void run() {
                setInitialFocus();
            }
        });
    }

    public void dismiss() {
        mDialog.dismiss();
    }

    public void disableAndWaitForVerification() {
        mCardUnmaskInput.setEnabled(false);
        mMonthSpinner.setEnabled(false);
        mYearSpinner.setEnabled(false);

        mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(false);

        mVerificationProgressBar.setVisibility(View.VISIBLE);
        mVerificationView.setVisibility(View.GONE);
    }

    public void verificationFinished(boolean success) {
        mVerificationProgressBar.setVisibility(View.GONE);
        if (!success) {
            TextView message = mVerificationView;
            message.setText("Verification failed. Please try again.");
            message.setVisibility(View.VISIBLE);
            mCardUnmaskInput.setEnabled(true);
            mMonthSpinner.setEnabled(true);
            mYearSpinner.setEnabled(true);
            setInitialFocus();
            // TODO(estade): UI decision - should we clear the input?
        } else {
            mDialog.findViewById(R.id.verification_success).setVisibility(View.VISIBLE);
            Handler h = new Handler();
            h.postDelayed(new Runnable() {
                public void run() {
                    dismiss();
                }
            }, 500);
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        mDelegate.dismissed();
    }

    @Override
    public void afterTextChanged(Editable s) {
        mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(areInputsValid());
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {}

    private void initializeExpirationDateSpinners() {
        ArrayAdapter<CharSequence> monthAdapter = new ArrayAdapter<CharSequence>(
                mDialog.getContext(), android.R.layout.simple_spinner_item);

        // TODO(estade): i18n, or remove this entry, or something.
        monthAdapter.add("MM");
        NumberFormat nf = NumberFormat.getInstance();
        nf.setMinimumIntegerDigits(2);
        for (int month = 1; month <= 12; month++) {
            monthAdapter.add(nf.format(month));
        }
        monthAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mMonthSpinner.setAdapter(monthAdapter);

        ArrayAdapter<CharSequence> yearAdapter = new ArrayAdapter<CharSequence>(
                mDialog.getContext(), android.R.layout.simple_spinner_item);
        yearAdapter.add("YYYY");
        Calendar calendar = Calendar.getInstance();
        int initialYear = calendar.get(Calendar.YEAR);
        for (int year = initialYear; year < initialYear + 10; year++) {
            yearAdapter.add(Integer.toString(year));
        }
        yearAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mYearSpinner.setAdapter(yearAdapter);

        mMonthSpinner.setVisibility(View.VISIBLE);
        mYearSpinner.setVisibility(View.VISIBLE);
    }

    private void setInitialFocus() {
        if (mShouldRequestExpirationDate) return;

        InputMethodManager imm = (InputMethodManager) mDialog.getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        imm.showSoftInput(mCardUnmaskInput, InputMethodManager.SHOW_IMPLICIT);
    }

    private boolean areInputsValid() {
        if (mShouldRequestExpirationDate
                && (mMonthSpinner.getSelectedItemPosition() == 0
                        || mYearSpinner.getSelectedItemPosition() == 0)) {
            return false;
        }
        return mDelegate.checkUserInputValidity(mCardUnmaskInput.getText().toString());
    }
}
