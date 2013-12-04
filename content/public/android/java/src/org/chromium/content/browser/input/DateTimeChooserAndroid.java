// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content.browser.ContentViewCore;

/**
 * Plumbing for the different date/time dialog adapters.
 */
@JNINamespace("content")
class DateTimeChooserAndroid {

    private final long mNativeDateTimeChooserAndroid;
    private final InputDialogContainer mInputDialogContainer;

    private DateTimeChooserAndroid(Context context,
            long nativeDateTimeChooserAndroid) {
        mNativeDateTimeChooserAndroid = nativeDateTimeChooserAndroid;
        mInputDialogContainer = new InputDialogContainer(context,
                new InputDialogContainer.InputActionDelegate() {

            @Override
            public void replaceDateTime(double value) {
                nativeReplaceDateTime(mNativeDateTimeChooserAndroid, value);
            }

            @Override
            public void cancelDateTimeDialog() {
                nativeCancelDialog(mNativeDateTimeChooserAndroid);
            }
        });
    }

    private void showDialog(int dialogType, double dialogValue,
                            double min, double max, double step) {
        mInputDialogContainer.showDialog(dialogType, dialogValue, min, max, step);
    }

    @CalledByNative
    private static DateTimeChooserAndroid createDateTimeChooser(
            ContentViewCore contentViewCore,
            long nativeDateTimeChooserAndroid,
            int dialogType, double dialogValue,
            double min, double max, double step) {
        DateTimeChooserAndroid chooser =
                new DateTimeChooserAndroid(
                        contentViewCore.getContext(),
                        nativeDateTimeChooserAndroid);
        chooser.showDialog(dialogType, dialogValue, min, max, step);
        return chooser;
    }

    @CalledByNative
    private static void initializeDateInputTypes(
            int textInputTypeDate, int textInputTypeDateTime,
            int textInputTypeDateTimeLocal, int textInputTypeMonth,
            int textInputTypeTime, int textInputTypeWeek) {
        InputDialogContainer.initializeInputTypes(textInputTypeDate,
                textInputTypeDateTime, textInputTypeDateTimeLocal,
                textInputTypeMonth, textInputTypeTime, textInputTypeWeek);
    }

    private native void nativeReplaceDateTime(long nativeDateTimeChooserAndroid,
                                              double dialogValue);

    private native void nativeCancelDialog(long nativeDateTimeChooserAndroid);
}
