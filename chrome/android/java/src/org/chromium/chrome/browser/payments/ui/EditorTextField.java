// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.os.Build;
import android.support.design.widget.TextInputLayout;
import android.support.v7.widget.AppCompatAutoCompleteTextView;
import android.telephony.PhoneNumberFormattingTextWatcher;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;

import javax.annotation.Nullable;

/** Handles validation and display of one field from the {@link EditorFieldModel}. */
@VisibleForTesting
public class EditorTextField extends TextInputLayout {

    /** The indicator for input fields that are required. */
    private static final String REQUIRED_FIELD_INDICATOR = "*";

    private EditorFieldModel mEditorFieldModel;
    private AutoCompleteTextView mInput;
    private boolean mHasFocusedAtLeastOnce;
    private PaymentRequestObserverForTest mObserverForTest;
    private boolean mIsBackgroundMutated;

    public EditorTextField(Context context, final EditorFieldModel fieldModel,
            OnEditorActionListener actionlistener, PhoneNumberFormattingTextWatcher formatter,
            PaymentRequestObserverForTest observer) {
        super(context);
        assert fieldModel.getInputTypeHint() != EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN;
        mEditorFieldModel = fieldModel;
        mObserverForTest = observer;

        // Build up the label.  Required fields are indicated by appending a '*'.
        CharSequence label = fieldModel.getLabel();
        if (fieldModel.isRequired()) label = label + REQUIRED_FIELD_INDICATOR;
        setHint(label);

        // The TextView is a child of this class.  The TextInputLayout manages how it looks.
        mInput = new AppCompatAutoCompleteTextView(getContext());
        addView(mInput);

        mInput.setText(fieldModel.getValue());
        mInput.setOnEditorActionListener(actionlistener);
        mInput.getLayoutParams().width = LayoutParams.MATCH_PARENT;
        mInput.getLayoutParams().height = LayoutParams.WRAP_CONTENT;

        // Validate the field when the user de-focuses it.
        mInput.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (hasFocus) {
                    mHasFocusedAtLeastOnce = true;
                } else if (mHasFocusedAtLeastOnce) {
                    // Show no errors until the user has already tried to edit the field once.
                    updateDisplayedError(!mEditorFieldModel.isValid());
                }
            }
        });

        // Update the model as the user edits the field.
        mInput.addTextChangedListener(new TextWatcher() {
            @Override
            public void afterTextChanged(Editable s) {
                fieldModel.setValue(s.toString());
                updateDisplayedError(false);
                if (mObserverForTest != null) {
                    mObserverForTest.onPaymentRequestEditorTextUpdate();
                }
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}
        });

        // Display any autofill suggestions.
        if (fieldModel.getSuggestions() != null && !fieldModel.getSuggestions().isEmpty()) {
            mInput.setAdapter(new ArrayAdapter<CharSequence>(getContext(),
                    android.R.layout.simple_spinner_dropdown_item,
                    fieldModel.getSuggestions()));
            mInput.setThreshold(0);
        }

        switch (fieldModel.getInputTypeHint()) {
            case EditorFieldModel.INPUT_TYPE_HINT_PHONE:
                mInput.setInputType(InputType.TYPE_CLASS_PHONE);
                break;
            case EditorFieldModel.INPUT_TYPE_HINT_EMAIL:
                mInput.setInputType(InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
                break;
            case EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES:
                // TODO(rouslan): Provide a hint to the keyboard that the street lines are
                // likely to have numbers.
                mInput.setInputType(InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE
                        | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS);
                break;
            case EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME:
                mInput.setInputType(InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        | InputType.TYPE_TEXT_VARIATION_PERSON_NAME);
                break;
            case EditorFieldModel.INPUT_TYPE_HINT_REGION:
                mInput.setInputType(InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
                        | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS);
                break;
            case EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC:
                // Intentionally fall through.
                // TODO(rouslan): Provide a hint to the keyboard that postal code and sorting
                // code are likely to have numbers.
            default:
                mInput.setInputType(InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS);
                break;
        }
    }

    /**
     * Super gross, dirty, awful hack for dealing with bugs in version 23 of the support library.
     *
     * Gleaned using dirty things from comments on the Android bug and support library source:
     * https://code.google.com/p/android/issues/detail?id=190829
     *
     * TODO(dfalcantara): Remove this super gross dirty hack once Chrome can roll version 24:
     *                    https://crbug.com/603635
     */
    @Override
    public void setError(@Nullable CharSequence error) {
        if (!mIsBackgroundMutated && getEditText() != null && getEditText().getBackground() != null
                && ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP))) {
            getEditText().setBackground(
                    getEditText().getBackground().getConstantState().newDrawable());
            getEditText().getBackground().mutate();
            mIsBackgroundMutated = true;
        }

        super.setError(error);
    }

    /** @return The EditorFieldModel that the TextView represents. */
    public EditorFieldModel getFieldModel() {
        return mEditorFieldModel;
    }

    /** @return The TextView for the field. */
    @Override
    public AutoCompleteTextView getEditText() {
        return mInput;
    }

    /**
     * Updates the error display.
     *
     * @param showError If true, displays the error message.  If false, clears it.
     */
    public void updateDisplayedError(boolean showError) {
        setError(showError ? mEditorFieldModel.getErrorMessage() : null);
        setErrorEnabled(showError);
    }
}
