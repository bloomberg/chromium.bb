// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.graphics.Bitmap;
import android.os.Handler;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.autofill.AutofillDialog.AutofillDialogDelegate;
import org.chromium.ui.ViewAndroid;
import org.chromium.ui.ViewAndroidDelegate;
import org.chromium.ui.WindowAndroid;

import org.chromium.chrome.R;

/**
* JNI call glue for AutofillDialog C++ and Java objects.
*/
@JNINamespace("autofill")
public class AutofillDialogGlue implements AutofillDialogDelegate,
        AutofillDialogAccountHelper.SignInContinuation {
    @SuppressWarnings("unused")
    private final AutofillDialog mAutofillDialog;
    private final AutofillDialogAccountHelper mAccountHelper;
    private int mNativeDialogPopup;  // could be 0 after onDestroy().
    private ViewAndroid mViewAndroid;

    public AutofillDialogGlue(int nativeAutofillDialogViewAndroid, WindowAndroid windowAndroid) {
        mNativeDialogPopup = nativeAutofillDialogViewAndroid;

        mAccountHelper = new AutofillDialogAccountHelper(this, windowAndroid.getContext());

        mAutofillDialog = new AutofillDialog(windowAndroid.getContext(), this);
        mViewAndroid = new ViewAndroid(windowAndroid, mAutofillDialog);
        mAutofillDialog.show();
    }

    @CalledByNative
    private static AutofillDialogGlue create(int nativeAutofillDialogViewAndroid,
            WindowAndroid windowAndroid) {
        return new AutofillDialogGlue(nativeAutofillDialogViewAndroid, windowAndroid);
    }

    @CalledByNative
    private void onDestroy() {
        if (mNativeDialogPopup == 0) return;

        mNativeDialogPopup = 0;
        mAutofillDialog.dismissAutofillDialog();
    }

    @CalledByNative
    private void dismissDialog() {
        mAutofillDialog.dismissAutofillDialog();
    }

    /**
     * @see AutofillDialog#updateNotificationArea(AutofillDialogNotification[])
     */
    @CalledByNative
    private void updateNotificationArea(AutofillDialogNotification[] notifications) {
        assert mNativeDialogPopup != 0;
        if (mNativeDialogPopup == 0) return;

        mAutofillDialog.updateNotificationArea(notifications);
    }

    /**
     * @see AutofillDialog#updateSection(int, boolean, AutofillDialogField[],
     *                                   AutofillDialogMenuItem[], int, boolean, int)
     */
    @CalledByNative
    private void updateSection(int section, boolean visible, AutofillDialogField[] dialogInputs,
            AutofillDialogMenuItem[] menuItems, int selectedMenuItem,
            boolean clobberInputs, int fieldTypeToAlwaysClobber) {
        mAutofillDialog.updateSection(
                section, visible, dialogInputs, menuItems, selectedMenuItem,
                clobberInputs, fieldTypeToAlwaysClobber);
    }

    /**
     * @see AutofillDialog#updateSectionErrors(int, AutofillDialogFieldError[])
     */
    @CalledByNative
    private void updateSectionErrors(int section, AutofillDialogFieldError[] errors) {
        mAutofillDialog.updateSectionErrors(section, errors);
    }

    /**
     * @see AutofillDialog#modelChanged(boolean)
     */
    @CalledByNative
    private void modelChanged(boolean fetchingIsActive) {
        mAutofillDialog.modelChanged(fetchingIsActive);
    }

    /**
     * @see AutofillDialog#updateAccountChooserAndAddTitle(String[], int)
     */
    @CalledByNative
    private void updateAccountChooser(String[] accountNames, int selectedAccountIndex) {
        mAutofillDialog.updateAccountChooser(accountNames, selectedAccountIndex);
    }

    /**
     * @see AutofillDialog#getSection(int)
     */
    @CalledByNative
    private AutofillDialogField[] getSection(int section) {
        return mAutofillDialog.getSection(section);
    }

    /**
     * @see AutofillDialog#getCvc()
     */
    @CalledByNative
    private String getCvc() {
        return mAutofillDialog.getCvc();
    }

    /**
     * @see AutofillDialog#shouldSaveDetailsLocally()
     */
    @CalledByNative
    private boolean shouldSaveDetailsLocally() {
        return mAutofillDialog.shouldSaveDetailsLocally();
    }

    /**
     * @see AutofillDialog#updateProgressBar(double)
     */
    @CalledByNative
    private void updateProgressBar(double value) {
        mAutofillDialog.updateProgressBar(value);
    }

    /**
     * Starts an automatic sign-in attempt for a given account.
     * @param accountName An account name (email) to sign into.
     */
    @CalledByNative
    private void startAutomaticSignIn(String accountName) {
        mAccountHelper.startTokensGeneration(accountName);
    }

    /**
     * @return An array of Google account emails the user has.
     */
    @CalledByNative
    private String[] getUserAccountNames() {
        return mAccountHelper.getAccountNames();
    }

    /**
     * @see AutofillDialog#offerToSaveLocally()
     */
    @CalledByNative
    private void updateSaveLocallyCheckBox(boolean shouldShow) {
        mAutofillDialog.updateSaveLocallyCheckBox(shouldShow);
    }

    // AutofillDialogAccountHelper.SignInContinuation implementation.
    @Override
    public void onTokensGenerationSuccess(String accountName, String sid, String lsid) {
        nativeContinueAutomaticSignin(mNativeDialogPopup, accountName, sid, lsid);
    }

    @Override
    public void onTokensGenerationFailure() {
        nativeContinueAutomaticSignin(mNativeDialogPopup, "", "", "");
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
    public void notificationCheckboxStateChanged(int type, boolean checked) {
        nativeNotificationCheckboxStateChanged(mNativeDialogPopup, type, checked);
    }

    @Override
    public void editingStart(int section) {
        nativeEditingStart(mNativeDialogPopup, section);
    }

    @Override
    public boolean editingComplete(int section) {
        return nativeEditingComplete(mNativeDialogPopup, section);
    }

    @Override
    public void editingCancel(int section) {
        nativeEditingCancel(mNativeDialogPopup, section);
    }

    @Override
    public void editedOrActivatedField(int dialogInputPointer, ViewAndroidDelegate delegate,
            String value, boolean wasEdit) {
        nativeEditedOrActivatedField(mNativeDialogPopup, dialogInputPointer,
                mViewAndroid.getNativePointer(), value, wasEdit);
    }

    @Override
    public String validateField(int fieldType, String value) {
        return nativeValidateField(mNativeDialogPopup, fieldType, value);
    }

    @Override
    public void validateSection(int section) {
        nativeValidateSection(mNativeDialogPopup, section);
    }

    @Override
    public void dialogSubmit() {
        assert mNativeDialogPopup != 0;
        if (mNativeDialogPopup == 0) return;

        nativeDialogSubmit(mNativeDialogPopup);
    }

    @Override
    public void dialogCancel() {
        assert mNativeDialogPopup != 0;
        if (mNativeDialogPopup == 0) return;

        nativeDialogCancel(mNativeDialogPopup);
    }

    @Override
    public void dialogDismissed() {
        if (mNativeDialogPopup == 0) return;

        // The controller doesn't expect to get deleted synchronously, so
        // we postpone the call.  onDestroy might be called before, so we
        // need to check if the native side is still alive.
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                if (mNativeDialogPopup != 0) nativeDialogDismissed(mNativeDialogPopup);
            }
        });
    }

    @Override
    public Bitmap getIconForField(int fieldType, String input) {
        return nativeGetIconForField(mNativeDialogPopup, fieldType, input);
    }

    @Override
    public String getPlaceholderForField(int section, int fieldType) {
        return nativeGetPlaceholderForField(mNativeDialogPopup, section, fieldType);
    }

    @Override
    public String getLabelForSection(int section) {
        return nativeGetLabelForSection(mNativeDialogPopup, section);
    }

    @Override
    public String[] getListForField(int field) {
        return nativeGetListForField(mNativeDialogPopup, field);
    }

    @Override
    public String getDialogButtonText(int dialogButtonId) {
        // TODO(aruslan): to be removed once http://crbug.com/235493 is cleared.
        assert mNativeDialogPopup != 0;
        if (mNativeDialogPopup == 0) return null;

        return nativeGetDialogButtonText(mNativeDialogPopup, dialogButtonId);
    }

    @Override
    public boolean isDialogButtonEnabled(int dialogButtonId) {
        // TODO(aruslan): to be removed once http://crbug.com/235493 is cleared.
        assert mNativeDialogPopup != 0;
        if (mNativeDialogPopup == 0) return true;

        return nativeIsDialogButtonEnabled(mNativeDialogPopup, dialogButtonId);
    }

    @Override
    public String getSaveLocallyText() {
        // TODO(aruslan): to be removed once http://crbug.com/235493 is cleared.
        assert mNativeDialogPopup != 0;
        if (mNativeDialogPopup == 0) return null;

        return nativeGetSaveLocallyText(mNativeDialogPopup);
    }

    @Override
    public String getLegalDocumentsText() {
        return nativeGetLegalDocumentsText(mNativeDialogPopup);
    }

    @Override
    public String getProgressBarText() {
        return nativeGetProgressBarText(mNativeDialogPopup);
    }

    @Override
    public boolean isTheAddItem(int section, int position) {
        return nativeIsTheAddItem(mNativeDialogPopup, section, position);
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
    private static AutofillDialogFieldError[] createAutofillDialogFieldError(int size) {
        return new AutofillDialogFieldError[size];
    }

    @CalledByNative
    private static void addToAutofillDialogFieldErrorArray(AutofillDialogFieldError[] array,
            int index, int fieldType, String errorText) {
        array[index] = new AutofillDialogFieldError(fieldType, errorText);
    }

    @CalledByNative
    private static AutofillDialogMenuItem[] createAutofillDialogMenuItemArray(int size) {
        return new AutofillDialogMenuItem[size];
    }

    @CalledByNative
    private static void addToAutofillDialogMenuItemArray(AutofillDialogMenuItem[] array, int index,
            String line1, String line2, Bitmap icon, int buttonType) {
        int buttonLabelResourceId = -1;
        switch (buttonType) {
            case AutofillDialogConstants.MENU_ITEM_BUTTON_TYPE_NONE:
                break;
            case AutofillDialogConstants.MENU_ITEM_BUTTON_TYPE_ADD:
                buttonLabelResourceId = R.string.autofill_add_button;
                break;
            case AutofillDialogConstants.MENU_ITEM_BUTTON_TYPE_EDIT:
                buttonLabelResourceId = R.string.autofill_edit_button;
                break;
            default:
                assert false;
        }
        array[index] = new AutofillDialogMenuItem(
                index, line1, line2, icon,
                buttonType != AutofillDialogConstants.MENU_ITEM_BUTTON_TYPE_NONE,
                buttonLabelResourceId, null);
    }

    @CalledByNative
    private static AutofillDialogNotification[] createAutofillDialogNotificationArray(int size) {
        return new AutofillDialogNotification[size];
    }

    @CalledByNative
    private static void addToAutofillDialogNotificationArray(AutofillDialogNotification[] array,
            int index, int type, int backgroundColor, int textColor,
            boolean hasArrow, boolean hasCheckbox, boolean isChecked, boolean isInteractive,
            String text) {
        array[index] = new AutofillDialogNotification(type, backgroundColor, textColor,
                hasArrow, hasCheckbox, isChecked, isInteractive, text);
    }

    // Calls from Java to C++ AutofillDialogViewAndroid --------------------------------------------

    private native void nativeItemSelected(int nativeAutofillDialogViewAndroid, int section,
            int index);
    private native void nativeAccountSelected(int nativeAutofillDialogViewAndroid, int index);
    private native void nativeNotificationCheckboxStateChanged(
            int nativeAutofillDialogViewAndroid, int type, boolean checked);
    private native void nativeContinueAutomaticSignin(
            int nativeAutofillDialogViewAndroid,
            String accountName, String sid, String lsid);
    private native void nativeEditingStart(int nativeAutofillDialogViewAndroid, int section);
    private native boolean nativeEditingComplete(int nativeAutofillDialogViewAndroid, int section);
    private native void nativeEditingCancel(int nativeAutofillDialogViewAndroid, int section);
    private native void nativeEditedOrActivatedField(int nativeAutofillDialogViewAndroid,
            int dialogInputPointer, int viewAndroid, String value, boolean wasEdit);
    private native String nativeValidateField(int nativeAutofillDialogViewAndroid, int fieldType,
            String value);
    private native void nativeValidateSection(int nativeAutofillDialogViewAndroid, int section);
    private native void nativeDialogSubmit(int nativeAutofillDialogViewAndroid);
    private native void nativeDialogCancel(int nativeAutofillDialogViewAndroid);
    private native void nativeDialogDismissed(int nativeAutofillDialogViewAndroid);
    private native String nativeGetLabelForSection(int nativeAutofillDialogViewAndroid,
            int section);
    private native String[] nativeGetListForField(int nativeAutofillDialogViewAndroid, int field);
    private native Bitmap nativeGetIconForField(int nativeAutofillDialogViewAndroid,
            int fieldType, String input);
    private native String nativeGetPlaceholderForField(int nativeAutofillDialogViewAndroid,
            int section, int fieldType);
    private native String nativeGetDialogButtonText(int nativeAutofillDialogViewAndroid,
            int dialogButtonId);
    private native boolean nativeIsDialogButtonEnabled(int nativeAutofillDialogViewAndroid,
            int dialogButtonId);
    private native String nativeGetSaveLocallyText(int nativeAutofillDialogViewAndroid);
    private native String nativeGetLegalDocumentsText(int nativeAutofillDialogViewAndroid);
    private native String nativeGetProgressBarText(int nativeAutofillDialogViewAndroid);
    private native boolean nativeIsTheAddItem(
            int nativeAutofillDialogViewAndroid, int section, int index);
}
