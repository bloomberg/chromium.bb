// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.text.TextUtils;
import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.preferences.autofill.AutofillProfileBridge.DropdownKeyValue;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

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

    /** Indicates a multi-line address field that may include numbers. */
    public static final int INPUT_TYPE_HINT_STREET_LINES = 3;

    /** Indicates a person's name. */
    public static final int INPUT_TYPE_HINT_PERSON_NAME = 4;

    /** Indicates a region or an administrative area, e.g., a state or a province. */
    public static final int INPUT_TYPE_HINT_REGION = 5;

    /** Indicates an alpha-numeric value, e.g., postal code or sorting code. */
    public static final int INPUT_TYPE_HINT_ALPHA_NUMERIC = 6;

    /** Indicates a dropdown. */
    public static final int INPUT_TYPE_HINT_DROPDOWN = 7;

    private final int mInputTypeHint;
    @Nullable private final List<DropdownKeyValue> mDropdownKeyValues;
    @Nullable private final List<CharSequence> mSuggestions;
    @Nullable private final EditorFieldValidator mValidator;
    @Nullable private final CharSequence mInvalidErrorMessage;
    @Nullable private CharSequence mLabel;
    @Nullable private CharSequence mRequiredErrorMessage;
    @Nullable private CharSequence mErrorMessage;
    @Nullable private CharSequence mValue;
    @Nullable private Callback<Pair<String, Runnable>> mDropdownCallback;
    private boolean mIsFullLine = true;

    /**
     * Constructs a dropdown field model.
     *
     * @param label             The human-readable label for user to understand the type of data
     *                          that should be entered into this field.
     * @param dropdownKeyValues The keyed values to display in the dropdown.
     */
    public EditorFieldModel(CharSequence label, List<DropdownKeyValue> dropdownKeyValues) {
        mInputTypeHint = INPUT_TYPE_HINT_DROPDOWN;
        mDropdownKeyValues = dropdownKeyValues;
        mSuggestions = null;
        mValidator = null;
        mInvalidErrorMessage = null;
        mLabel = label;
    }

    /**
     * Constructs a text input field model.
     *
     * @param inputTypeHint The type of input. For example, INPUT_TYPE_HINT_PHONE.
     */
    public EditorFieldModel(int inputTypeHint) {
        assert inputTypeHint != INPUT_TYPE_HINT_DROPDOWN;
        mInputTypeHint = inputTypeHint;
        mDropdownKeyValues = null;
        mSuggestions = null;
        mValidator = null;
        mInvalidErrorMessage = null;
        mLabel = null;
    }

    /**
     * Constructs a text input field model.
     *
     * @param inputTypeHint        The type of input. For example, INPUT_TYPE_HINT_PHONE.
     * @param label                The human-readable label for user to understand the type of data
     *                             that should be entered into this field.
     * @param suggestions          Optionally empty set of values to suggest to the user.
     * @param validator            Optional validator for the values in this field.
     * @param requiredErrorMessage The optional error message that indicates to the user that they
     *                             cannot leave this field empty.
     * @param invalidErrorMessage  The optional error message that indicates to the user that the
     *                             value they have entered is not valid.
     * @param value                Optional initial value of this field.
     */
    public EditorFieldModel(int inputTypeHint, CharSequence label,
            Set<CharSequence> suggestions, @Nullable EditorFieldValidator validator,
            @Nullable CharSequence requiredErrorMessage, @Nullable CharSequence invalidErrorMessage,
            @Nullable CharSequence value) {
        assert inputTypeHint != INPUT_TYPE_HINT_DROPDOWN;
        assert suggestions != null;
        mInputTypeHint = inputTypeHint;
        mDropdownKeyValues = null;
        mSuggestions = new ArrayList<CharSequence>(suggestions);
        mValidator = validator;
        mInvalidErrorMessage = invalidErrorMessage;
        mLabel = label;
        mRequiredErrorMessage = requiredErrorMessage;
        mValue = value;
    }

    /** @return The type of input, for example, INPUT_TYPE_HINT_PHONE. */
    public int getInputTypeHint() {
        return mInputTypeHint;
    }

    /** @return The spinner key values. */
    public List<DropdownKeyValue> getDropdownKeyValues() {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        return mDropdownKeyValues;
    }

    /** @return The human-readable label for this field. */
    public CharSequence getLabel() {
        return mLabel;
    }

    /**
     * Updates the label.
     *
     * @param label The new label to use.
     */
    public void setLabel(CharSequence label) {
        mLabel = label;
    }

    /** @return Suggested values for this field. Can be null. */
    @Nullable public List<CharSequence> getSuggestions() {
        return mSuggestions;
    }

    /** @return The error message for the last validation. Can be null if no error was reported. */
    @Nullable public CharSequence getErrorMessage() {
        return mErrorMessage;
    }

    /** @return The value that the user has typed into the field or the key of the value that the
     *          user has selected in the dropdown. Can be null. */
    @Nullable public CharSequence getValue() {
        return mValue;
    }

    /**
     * Updates the value of this field. Does not trigger validation or update the last error
     * message. Can be called on a dropdown to initialize it, but will not fire the dropdown
     * callback.
     *
     * @param value The new value that the user has typed in or the initial key for the dropdown.
     */
    public void setValue(@Nullable CharSequence userTypedValueOrInitialDropdownKey) {
        mValue = userTypedValueOrInitialDropdownKey;
    }

    /**
     * Updates the dropdown selection and fires the dropdown callback.
     *
     * @param key      The new dropdown key.
     * @param callback The callback to invoke when the change has been processed.
     */
    public void setDropdownKey(String key, Runnable callback) {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        mValue = key;
        if (mDropdownCallback != null) {
            mDropdownCallback.onResult(new Pair<String, Runnable>(key, callback));
        }
    }

    /** @return Whether or not the field is required. */
    public boolean isRequired() {
        return !TextUtils.isEmpty(mRequiredErrorMessage);
    }

    /**
     * Updates the required error message.
     *
     * @param message The error message to use if this field is required, but empty. If null, then
     *                this field is optional.
     */
    public void setRequiredErrorMessage(@Nullable CharSequence message) {
        mRequiredErrorMessage = message;
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

    /**
     * Sets the dropdown callback.
     *
     * @param callback The callback to invoke when the dropdown selection has changed. The first
     *                 element in the callback pair is the dropdown key. The second element is the
     *                 callback to invoke after the dropdown change has been processed.
     */
    public void setDropdownCallback(Callback<Pair<String, Runnable>> callback) {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        mDropdownCallback = callback;
    }

    /** @return True if the input field should take up the full line, instead of sharing with other
     *          input fields. This is the default.*/
    public boolean isFullLine() {
        return mIsFullLine;
    }

    /**
     * Sets whether this input field should take up the full line. All fields take up the full line
     * by default.
     *
     * @param isFullLine Whether the input field should take up the full line.
     */
    public void setIsFullLine(boolean isFullLine) {
        mIsFullLine = isFullLine;
    }
}
