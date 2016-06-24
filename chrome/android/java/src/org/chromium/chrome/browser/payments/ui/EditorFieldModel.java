// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.text.TextUtils;

import java.util.List;

import javax.annotation.Nullable;

/**
 * Representation of a single input text field in an editor. Can be used, for example, for a phone
 * input field.
 */
public class EditorFieldModel {
    /**
     * The interface to be implemented by the field validator.
     */
    public interface EditorFieldValidator {
        /**
         * Called to check the validity of the field value.
         *
         * @param value The value of the field to check.
         * @return True if the value is valid.
         */
        boolean isValid(@Nullable CharSequence value);
    }

    /** Indicates a phone field. */
    public static final int INPUT_TYPE_HINT_PHONE = 1;

    /** Indicates an email field. */
    public static final int INPUT_TYPE_HINT_EMAIL = 2;

    private final int mInputTypeHint;
    private final CharSequence mLabel;
    @Nullable private final List<CharSequence> mSuggestions;
    @Nullable private final EditorFieldValidator mValidator;
    @Nullable private final CharSequence mRequiredErrorMessage;
    @Nullable private final CharSequence mInvalidErrorMessage;
    @Nullable private CharSequence mErrorMessage;
    @Nullable private CharSequence mValue;

    /**
     * Constructs a field model.
     *
     * @param inputTypeHint        The type of input. For example, INPUT_TYPE_HINT_PHONE.
     * @param label                The human-readable label for user to understand the type of data
     *                             that should be entered into this field.
     * @param suggestions          Optional list of values to suggest to the user.
     * @param validator            Optional validator for the values in this field.
     * @param requiredErrorMessage The optional error message that indicates to the user that they
     *                             cannot leave this field empty.
     * @param invalidErrorMessage  The optional error message that indicates to the user that the
     *                             value they have entered is not valid.
     * @param value                Optional initial value of this field.
     */
    public EditorFieldModel(int inputTypeHint, CharSequence label,
            @Nullable List<CharSequence> suggestions, @Nullable EditorFieldValidator validator,
            @Nullable CharSequence requiredErrorMessage, @Nullable CharSequence invalidErrorMessage,
            @Nullable CharSequence value) {
        mInputTypeHint = inputTypeHint;
        mLabel = label;
        mSuggestions = suggestions;
        mValidator = validator;
        mRequiredErrorMessage = requiredErrorMessage;
        mInvalidErrorMessage = invalidErrorMessage;
        mValue = value;
    }

    /** @return The type of input, for example, INPUT_TYPE_HINT_PHONE. */
    public int getInputTypeHint() {
        return mInputTypeHint;
    }

    /** @return The human-readable label for this field. */
    public CharSequence getLabel() {
        return mLabel;
    }

    /** @return Suggested values for this field. Can be null. */
    @Nullable public List<CharSequence> getSuggestions() {
        return mSuggestions;
    }

    /** @return The error message for the last validation. Can be null if no error was reported. */
    @Nullable public CharSequence getErrorMessage() {
        return mErrorMessage;
    }

    /** @return The value that the user has entered into the field. Can be null. */
    @Nullable public CharSequence getValue() {
        return mValue;
    }

    /**
     * Updates the value of this field. Does not trigger validation or update the last error
     * message.
     *
     * @param value The new value that the user has entered.
     */
    public void setValue(@Nullable CharSequence value) {
        mValue = value;
    }

    /** @return Whether or not the field is required. */
    public boolean isRequired() {
        return !TextUtils.isEmpty(mRequiredErrorMessage);
    }

    /**
     * Returns true if the field value is valid. Also updates the error message.
     *
     * @return Whether the field value is valid.
     */
    public boolean isValid() {
        if (isRequired()
                && (TextUtils.isEmpty(mValue) || TextUtils.getTrimmedLength(mValue) == 0)) {
            mErrorMessage = mRequiredErrorMessage;
            return false;
        }

        if (mValidator != null && !mValidator.isValid(mValue)) {
            mErrorMessage = mInvalidErrorMessage;
            return false;
        }

        mErrorMessage = null;
        return true;
    }
}
