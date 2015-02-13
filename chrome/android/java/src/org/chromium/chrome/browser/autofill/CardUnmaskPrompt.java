// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.os.Build;
import android.os.Handler;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
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
    private final TextView mErrorMessage;
    private final View mMainContents;
    private final View mVerificationOverlay;
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
        mErrorMessage = (TextView) v.findViewById(R.id.error_message);
        mMainContents = v.findViewById(R.id.main_contents);
        mVerificationOverlay = v.findViewById(R.id.verification_overlay);
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

        // Calling this from here clobbers the input's background shadow, which is otherwise
        // highly resistant to styling.
        setInputError(null);
    }

    public void dismiss() {
        mDialog.dismiss();
    }

    public void disableAndWaitForVerification() {
        setInputsEnabled(false);
        mVerificationProgressBar.setVisibility(View.VISIBLE);
        // TODO(estade): l10n
        mVerificationView.setText("Verifying card");
        setInputError(null);
    }

    public void verificationFinished(boolean success) {
        if (!success) {
            setInputsEnabled(true);
            setInputError("Credit card could not be verified. Try again.");
            setInitialFocus();
            // TODO(estade): UI decision - should we clear the input?
        } else {
            mVerificationProgressBar.setVisibility(View.GONE);
            mDialog.findViewById(R.id.verification_success).setVisibility(View.VISIBLE);
            mVerificationView.setText("Your card is verified");
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

    /**
     * Sets the enabled state of the main contents, and hides or shows the verification overlay.
     * @param enabled True if the inputs should be useable, false if the verification overlay
     *        obscures them.
     */
    private void setInputsEnabled(boolean enabled) {
        mCardUnmaskInput.setEnabled(enabled);
        mMonthSpinner.setEnabled(enabled);
        mYearSpinner.setEnabled(enabled);
        mMainContents.setAlpha(enabled ? 1.0f : 0.15f);
        mMainContents.setImportantForAccessibility(
                enabled ? View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
                        : View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        ((ViewGroup) mMainContents).setDescendantFocusability(
                enabled ? ViewGroup.FOCUS_BEFORE_DESCENDANTS
                        : ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(enabled);

        mVerificationOverlay.setVisibility(enabled ? View.GONE : View.VISIBLE);
    }

    /**
     * Sets the error message on the cvc input.
     * @param message The error message to show, or null if the error state should be cleared.
     */
    private void setInputError(String message) {
        mErrorMessage.setText(message);
        mErrorMessage.setVisibility(message == null ? View.GONE : View.VISIBLE);

        // The rest of this code makes L-specific assumptions about the background being used to
        // draw the TextInput.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        // The input is always active or in an error state. Simply clearing the color
        // filter resets the color to input_underline_color, even when it's active.
        int strokeColor = mDialog.getContext().getResources().getColor(
                message == null ? R.color.light_active_color
                                : R.color.input_underline_error_color);

        mCardUnmaskInput.getBackground().mutate().setColorFilter(
                new PorterDuffColorFilter(strokeColor, PorterDuff.Mode.SRC_IN));
    }
}
