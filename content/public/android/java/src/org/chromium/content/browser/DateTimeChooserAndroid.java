// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import android.content.Context;

/**
 * Plumbing for the different date/time dialog adapters.
 */
@JNINamespace("content")
class DateTimeChooserAndroid {

    private final int mNativeDateTimeChooserAndroid;
    private final InputDialogContainer mInputDialogContainer;

    private DateTimeChooserAndroid(Context context,
            int nativeDateTimeChooserAndroid) {
        mNativeDateTimeChooserAndroid = nativeDateTimeChooserAndroid;
        mInputDialogContainer = new InputDialogContainer(context,
                new InputDialogContainer.InputActionDelegate() {

            @Override
            public void replaceDateTime(String text) {
                nativeReplaceDateTime(mNativeDateTimeChooserAndroid, text);
            }

            @Override
            public void cancelDateTimeDialog() {
                nativeCancelDialog(mNativeDateTimeChooserAndroid);
            }
        });
    }

    private void showDialog(int dialogType, String text) {
        mInputDialogContainer.showDialog(text, dialogType);
    }

    @CalledByNative
    private static DateTimeChooserAndroid createDateTimeChooser(
            ContentViewCore contentViewCore,
            int nativeDateTimeChooserAndroid, String text, int dialogType) {
        DateTimeChooserAndroid chooser =
                new DateTimeChooserAndroid(
                        contentViewCore.getContext(), nativeDateTimeChooserAndroid);
        chooser.showDialog(dialogType, text);
        return chooser;
    }

    @CalledByNative
    private static void initializeDateInputTypes(int textInputTypeDate, int textInputTypeDateTime,
            int textInputTypeDateTimeLocal, int textInputTypeMonth,
            int textInputTypeTime) {
        InputDialogContainer.initializeInputTypes(textInputTypeDate, textInputTypeDateTime,
                textInputTypeDateTimeLocal, textInputTypeMonth, textInputTypeTime);
    }

    private native void nativeReplaceDateTime(int nativeDateTimeChooserAndroid, String text);
    private native void nativeCancelDialog(int nativeDateTimeChooserAndroid);
}
