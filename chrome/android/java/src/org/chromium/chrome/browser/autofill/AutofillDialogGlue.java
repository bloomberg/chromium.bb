// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.graphics.Bitmap;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.autofill.AutofillDialog.AutofillDialogDelegate;
import org.chromium.ui.gfx.NativeWindow;

/**
* JNI call glue for AutofillDialog C++ and Java objects.
*/
@JNINamespace("autofill")
public class AutofillDialogGlue implements AutofillDialogDelegate {
    @SuppressWarnings("unused")
    private final int mNativeDialogPopup;
    private AutofillDialog mAutofillDialog;

    public AutofillDialogGlue(int nativeAutofillDialogViewAndroid, NativeWindow nativeWindow) {
        mNativeDialogPopup = nativeAutofillDialogViewAndroid;

        mAutofillDialog = new AutofillDialog(nativeWindow.getContext());
        mAutofillDialog.show();
    }

    @CalledByNative
    private static AutofillDialogGlue create(int nativeAutofillDialogViewAndroid,
            NativeWindow nativeWindow) {
        return new AutofillDialogGlue(nativeAutofillDialogViewAndroid, nativeWindow);
    }

    /**
     * Updates a section of Autofill dialog.
     * @param section Section that needs to be updated. Should match one of the values in
     *                {@link AutofillDialogConstants}.
     * @param visible Whether the section should be visible.
     * @param dialogInputs Input fields of the currently selected menu item.
     * @param menuItems Menu items for the section.
     * @param selectedMenuItem The menu item that is currently selected or -1 otherwise.
     */
    @CalledByNative
    private void updateSection(int section, boolean visible, AutofillDialogField[] dialogInputs,
            AutofillDialogMenuItem[] menuItems, int selectedMenuItem) {
        mAutofillDialog.updateSection(section, visible, dialogInputs, menuItems, selectedMenuItem);
    }

    @CalledByNative
    private AutofillDialogField[] getSection(int section) {
        return mAutofillDialog.getSection(section);
    }

    @CalledByNative
    private String getCvc() {
        return mAutofillDialog.getCvc();
    }

    @CalledByNative
    private boolean shouldUseBillingForShipping() {
        return mAutofillDialog.shouldUseBillingForShipping();
    }

    @CalledByNative
    private boolean shouldSaveDetailsInWallet() {
        return mAutofillDialog.shouldSaveDetailsInWallet();
    }

    @CalledByNative
    private boolean shouldSaveDetailsLocally() {
        return mAutofillDialog.shouldSaveDetailsLocally();
    }

    /**
     * @param value The progress bar value in a range [0.0, 1.0]
     */
    @CalledByNative
    private void updateProgressBar(double value) {
        mAutofillDialog.updateProgressBar(value);
    }

    // AutofillDialogDelegate implementation ------------------------------------------------------

    @Override
    public void itemSelected(int section, int index) {
        nativeItemSelected(mNativeDialogPopup, section, index);
    }

    @Override
    public void accountSelected(int index) {
        nativeAccountSelected(mNativeDialogPopup, index);
    }

    @Override
    public void editingStart(int section) {
        nativeEditingStart(mNativeDialogPopup, section);
    }

    @Override
    public void editingComplete(int section) {
        nativeEditingComplete(mNativeDialogPopup, section);
    }

    @Override
    public void editingCancel(int section) {
        nativeEditingCancel(mNativeDialogPopup, section);
    }

    @Override
    public void dialogSubmit() {
        nativeDialogCancel(mNativeDialogPopup);
    }

    @Override
    public void dialogCancel() {
        nativeDialogCancel(mNativeDialogPopup);
    }

    // Helper methods for AutofillDialogField and AutofillDialogItem ------------------------------

    @CalledByNative
    private static AutofillDialogField[] createAutofillDialogFieldArray(int size) {
        return new AutofillDialogField[size];
    }

    /**
     * @param array AutofillDialogField array that should get a new field added.
     * @param index Index in the array where to place a new field.
     * @param nativePointer The pointer to the corresponding native field object.
     * @param fieldType The input field type. It should match one of the types in
     *                  {@link AutofillDialogConstants}.
     * @param placeholder The placeholder text for the input field. It can be an empty string.
     * @param value The value of the autofilled input field. It can be an empty string.
     */
    @CalledByNative
    private static void addToAutofillDialogFieldArray(AutofillDialogField[] array, int index,
            int nativePointer, int fieldType, String placeholder, String value) {
        array[index] = new AutofillDialogField(nativePointer, fieldType, placeholder, value);
    }

    @CalledByNative
    private static int getFieldNativePointer(AutofillDialogField field) {
        return field.mNativePointer;
    }

    @CalledByNative
    private static String getFieldValue(AutofillDialogField field) {
        return field.getValue();
    }

    @CalledByNative
    private static AutofillDialogMenuItem[] createAutofillDialogMenuItemArray(int size) {
        return new AutofillDialogMenuItem[size];
    }

    @CalledByNative
    private static void addToAutofillDialogMenuItemArray(AutofillDialogMenuItem[] array, int index,
            String line1, String line2, Bitmap icon) {
        array[index] = new AutofillDialogMenuItem(index, line1, line2, icon);
    }

    // Calls from Java to C++ AutofillDialogViewAndroid --------------------------------------------

    private native void nativeItemSelected(int nativeAutofillDialogViewAndroid, int section,
            int index);
    private native void nativeAccountSelected(int nativeAutofillDialogViewAndroid, int index);
    private native void nativeEditingStart(int nativeAutofillDialogViewAndroid, int section);
    private native void nativeEditingComplete(int nativeAutofillDialogViewAndroid, int section);
    private native void nativeEditingCancel(int nativeAutofillDialogViewAndroid, int section);
    private native void nativeDialogSubmit(int nativeAutofillDialogViewAndroid);
    private native void nativeDialogCancel(int nativeAutofillDialogViewAndroid);
}
