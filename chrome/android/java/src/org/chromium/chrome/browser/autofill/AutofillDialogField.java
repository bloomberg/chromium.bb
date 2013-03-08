// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

/**
 * Autofill dialog field container to store information needed for each Autofill dialog entry.
 */
public class AutofillDialogField {
    public final int mNativePointer;
    public final int mFieldType;
    public final String mPlaceholder;
    public final String mValue;

    /**
     * @param nativePointer The pointer to the corresponding native object.
     * @param fieldType The input field type that got determined by the server.
     * @param placeholder Placeholder text for this input field.
     * @param value Autofill value for this input field.
     */
    public AutofillDialogField(int nativePointer, int fieldType, String placeholder, String value) {
        mNativePointer = nativePointer;
        mFieldType = fieldType;
        mPlaceholder = placeholder;
        mValue = value;
    }
}
