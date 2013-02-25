// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.widget.Toast;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
* JNI call glue for AutofillDialog C++ and Java objects.
*/
@JNINamespace("autofill")
public class AutofillDialogGlue {
    @SuppressWarnings("unused")
    private final int mNativeDialogPopup;

    public AutofillDialogGlue(int nativeAutofillDialogViewAndroid, Context context) {
        mNativeDialogPopup = nativeAutofillDialogViewAndroid;

        Toast toast = Toast.makeText(context, "Interactive Autocomplete!", Toast.LENGTH_SHORT);
        toast.show();
    }

    @CalledByNative
    private static AutofillDialogGlue create(int nativeAutofillDialogViewAndroid, Context context) {
        return new AutofillDialogGlue(nativeAutofillDialogViewAndroid, context);
    }
}