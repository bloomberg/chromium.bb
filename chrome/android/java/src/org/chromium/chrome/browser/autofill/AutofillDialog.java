// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import java.util.ArrayList;
import java.util.List;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;

import android.content.DialogInterface.OnClickListener;
import android.text.TextUtils;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.EditText;
import android.widget.ScrollView;

import org.chromium.chrome.R;

/**
 * This is the dialog that will act as the java side controller between the
 * AutofillDialogGlue object and the UI elements on the page. It contains the
 * title and the content views and relays messages from the backend to them.
 */
public class AutofillDialog extends AlertDialog
        implements OnClickListener, OnItemSelectedListener {
    private AutofillDialogContentView mContentView;
    private AutofillDialogTitleView mTitleView;
    private AutofillDialogField[][] mAutofillSectionFieldData =
            new AutofillDialogField[AutofillDialogConstants.NUM_SECTIONS][];
    private AutofillDialogMenuItem[][] mAutofillSectionMenuData =
            new AutofillDialogMenuItem[AutofillDialogConstants.NUM_SECTIONS][];
    private boolean mWillDismiss = false;

    /**
     * An interface to handle the interaction with an AutofillDialog object.
     */
    public interface AutofillDialogDelegate {

        /**
         * Informs AutofillDialog controller that a menu item was selected.
         * @param section Section in which a menu item was selected. Should match one of the values
         *                in {@link AutofillDialogConstants}.
         * @param index Index of the selected menu item.
         */
        public void itemSelected(int section, int index);

        /**
         * Informs AutofillDialog controller that an account was selected.
         * @param index Index of the selected account.
         */
        public void accountSelected(int index);

        /**
         * Informs AutofillDialog controller that the view is starting editing.
         * @param section Section that is being edited. Should match one of the values in
         *                {@link AutofillDialogConstants}.
         */
        public void editingStart(int section);

        /**
         * Informs AutofillDialog controller that the view has completed editing.
         * @param section Section that was being edited. Should match one of the values in
         *                {@link AutofillDialogConstants}.
         */
        public void editingComplete(int section);

        /**
         * Informs AutofillDialog controller that the view has cancelled editing.
         * @param section Section that was being edited. Should match one of the values in
         *                {@link AutofillDialogConstants}.
         */
        public void editingCancel(int section);

        /**
         * Informs AutofillDialog controller that the user clicked on the submit button.
         */
        public void dialogSubmit();

        /**
         * Informs AutofillDialog controller that the user clicked on the cancel button.
         */
        public void dialogCancel();
    }

    protected AutofillDialog(Context context) {
        super(context);

        addTitleWithPlaceholders();

        ScrollView scroll = new ScrollView(context);
        mContentView = (AutofillDialogContentView) getLayoutInflater().
                inflate(R.layout.autofill_dialog_content, null);
        scroll.addView(mContentView);
        setView(scroll);

        setButton(AlertDialog.BUTTON_NEGATIVE,
                getContext().getResources().getString(R.string.autofill_negative_button), this);
        setButton(AlertDialog.BUTTON_POSITIVE,
                getContext().getResources().getString(R.string.autofill_positive_button), this);

        mContentView.setOnItemSelectedListener(this);
    }

    @Override
    public void dismiss() {
        if (mWillDismiss) super.dismiss();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (mContentView.getCurrentLayout() == AutofillDialogContentView.LAYOUT_STEADY) {
            mWillDismiss = true;
            return;
        }

        mContentView.changeLayoutTo(AutofillDialogContentView.LAYOUT_STEADY);
        getButton(BUTTON_POSITIVE).setText(R.string.autofill_positive_button);
        mWillDismiss = false;
    }

    @Override
    public void onItemSelected(AdapterView<?> spinner, View view, int position,
            long id) {
        if (spinner.getId() == R.id.accounts_spinner) {
            if (spinner.getItemAtPosition(position).toString().equals(
                    getContext().getResources().getString(R.string.autofill_use_local))) {
                mContentView.changeLayoutTo(AutofillDialogContentView.LAYOUT_STEADY);
            } else {
                mContentView.changeLayoutTo(AutofillDialogContentView.LAYOUT_FETCHING);
            }
            return;
        }

        int section = AutofillDialogUtils.getSectionForSpinnerID(spinner.getId());

        if (!mContentView.selectionShouldChangeLayout(spinner, section, position)) return;

        mContentView.changeLayoutTo(AutofillDialogContentView.getLayoutModeForSection(section));
        spinner.setSelection(0);
        getButton(BUTTON_POSITIVE).setText(R.string.autofill_positive_button_editing);
    }

    @Override
    public void onNothingSelected(AdapterView<?> spinner) {
    }

    //TODO(yusufo): Remove this. updateAccountChooserAndAddTitle will get initiated from glue.
    private void addTitleWithPlaceholders() {
        List<String> accounts = new ArrayList<String>();
        accounts.add("placeholder@gmail.com");
        accounts.add("placeholder@google.com");
        updateAccountChooserAndAddTitle(accounts);
    }

    /**
     * Update account chooser dropdown with given accounts and create the title view if needed.
     * @param accounts The accounts to be used for the dropdown.
     */
    public void updateAccountChooserAndAddTitle(List<String> accounts) {
        if (mTitleView == null) {
            mTitleView = new AutofillDialogTitleView(getContext(), accounts);
            mTitleView.setOnItemSelectedListener(this);
            setCustomTitle(mTitleView);
            return;
        }
        // TODO(yusufo): Add code to update accounts after title is created.
    }

    /**
     * Update given section with the data provided. This fills out the related {@link EditText}
     * fields in the layout corresponding to the section.
     * @param section The section to update with the given data.
     * @param visible Whether the section should be visible.
     * @param dialogInputs The array that contains the data for each field in the section.
     * @param menuItems The array that contains the dropdown items to be shown for the section.
     * @param selectedMenuItem The menu item that is currently selected or -1 otherwise.
     */
    public void updateSection(int section, boolean visible, AutofillDialogField[] dialogInputs,
            AutofillDialogMenuItem[] menuItems, int selectedMenuItem) {
        EditText currentField;
        String inputValue;
        for (int i = 0; i < dialogInputs.length; i++) {
            currentField = (EditText) findViewById(AutofillDialogUtils.getEditTextIDForField(
                    section, dialogInputs[i].mFieldType));
            if (currentField == null) continue;
            currentField.setHint(dialogInputs[i].mPlaceholder);
            inputValue = dialogInputs[i].getValue();
            if (TextUtils.isEmpty(inputValue)) currentField.setText("");
            else currentField.setText(inputValue);
        }
        mAutofillSectionFieldData[section] = dialogInputs;
        mContentView.setVisibilityForSection(section, visible);
        mContentView.updateMenuItemsForSection(section, menuItems);
        mAutofillSectionMenuData[section] = menuItems;
    }

    /**
     * Clears the field values for the given section.
     * @param section The section to clear the field values for.
     */
    public void clearAutofillSectionFieldValues(int section) {
        AutofillDialogField[] fieldData = mAutofillSectionFieldData[section];
        if (fieldData == null) return;

        for (int i = 0; i < fieldData.length; i++) fieldData[i].setValue("");
    }

    /**
     * Return the array that holds all the data about the fields in the given section.
     * @param section The section to return the data for.
     * @return An array containing the data for each field in the given section.
     */
    public AutofillDialogField[] getSection(int section) {
        AutofillDialogField[] fieldData = mAutofillSectionFieldData[section];
        if (fieldData == null) return null;

        EditText currentField;
        String currentValue;
        for (int i = 0; i < fieldData.length; i++) {
            currentField = (EditText) findViewById(AutofillDialogUtils.getEditTextIDForField(
                    section, fieldData[i].mFieldType));
            currentValue = currentField.getText().toString();
            if (TextUtils.isEmpty(currentValue)) continue;
            fieldData[i].setValue(currentValue);
        }
        return fieldData;
    }

     /**
      * @return The currently entered or previously saved CVC value in the dialog.
      */
    public String getCvc() {
        EditText cvcEdit = (EditText) findViewById(AutofillDialogUtils.getEditTextIDForField(
                AutofillDialogConstants.SECTION_CC,
                        AutofillDialogConstants.CREDIT_CARD_VERIFICATION_CODE));
        return cvcEdit.getText().toString();
    }

    /**
     * @return Whether the billing address should be used as shipping address.
     */
    public boolean shouldUseBillingForShipping() {
        return false;
    }

    /**
     * @return Whether the details entered should be saved to the currently active
     *         Wallet account.
     */
    public boolean shouldSaveDetailsInWallet() {
        return false;
    }

    /**
     * @return Whether the details entered should be saved locally on the device.
     */
    public boolean shouldSaveDetailsLocally() {
        return false;
    }

    /**
     * Updates the progress for the final transaction with the given value.
     * @param value The current progress percentage value.
     */
    public void updateProgressBar(double value) {
    }
}