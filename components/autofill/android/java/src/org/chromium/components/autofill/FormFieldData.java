// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * The wrap class of native autofill::FormFieldDataAndroid.
 */
@JNINamespace("autofill")
public class FormFieldData {
    public final String mLabel;
    public final String mName;
    public final String mAutocompleteAttr;
    public final boolean mShouldAutocomplete;
    public final String mPlaceholder;
    public final String mType;
    public final String mId;

    private String mValue;

    private FormFieldData(String name, String label, String value, String autocompleteAttr,
            boolean shouldAutocomplete, String placeholder, String type, String id) {
        mName = name;
        mLabel = label;
        mValue = value;
        mAutocompleteAttr = autocompleteAttr;
        mShouldAutocomplete = shouldAutocomplete;
        mPlaceholder = placeholder;
        mType = type;
        mId = id;
    }

    /**
     * @return value of field.
     */
    @CalledByNative
    public String getValue() {
        return mValue;
    }

    @CalledByNative
    public void updateValue(String value) {
        mValue = value;
    }

    @CalledByNative
    private static FormFieldData createFormFieldData(String name, String label, String value,
            String autocompleteAttr, boolean shouldAutocomplete, String placeholder, String type,
            String id) {
        return new FormFieldData(
                name, label, value, autocompleteAttr, shouldAutocomplete, placeholder, type, id);
    }
}
