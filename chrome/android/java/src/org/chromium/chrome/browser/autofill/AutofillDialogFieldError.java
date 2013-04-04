// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

/**
 * Autofill dialog field error container to store error text for an Autofill dialog entry.
 */
class AutofillDialogFieldError {
    final int mFieldType;
    final String mErrorText;

    public AutofillDialogFieldError(int fieldType, String errorText) {
        mFieldType = fieldType;
        mErrorText = errorText;
    }
}
