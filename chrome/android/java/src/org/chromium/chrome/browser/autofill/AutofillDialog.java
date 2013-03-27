// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;

import android.content.DialogInterface.OnClickListener;
import android.content.res.Resources;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * This is the dialog that will act as the java side controller between the
 * AutofillDialogGlue object and the UI elements on the page. It contains the
 * title and the content views and relays messages from the backend to them.
 */
public class AutofillDialog extends AlertDialog
        implements OnClickListener, OnItemSelectedListener {
    private static final int ADD_MENU_ITEM_INDEX = -1;
    private static final int EDIT_MENU_ITEM_INDEX = -2;

    private AutofillDialogContentView mContentView;
    private AutofillDialogTitleView mTitleView;
    private AutofillDialogField[][] mAutofillSectionFieldData =
            new AutofillDialogField[AutofillDialogConstants.NUM_SECTIONS][];
    private AutofillDialogMenuItem[][] mAutofillSectionMenuData =
            new AutofillDialogMenuItem[AutofillDialogConstants.NUM_SECTIONS][];
    private final List<String> mDefaultAccountItems = new ArrayList<String>();
    private final AutofillDialogMenuItem[][] mDefaultMenuItems =
            new AutofillDialogMenuItem[AutofillDialogConstants.NUM_SECTIONS][];
    private AutofillDialogDelegate mDelegate;
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

    protected AutofillDialog(Context context, AutofillDialogDelegate delegate) {
        super(context);
        mDelegate = delegate;

        addTitleWithPlaceholders();

        ScrollView scroll = new ScrollView(context);
        mContentView = (AutofillDialogContentView) getLayoutInflater().
                inflate(R.layout.autofill_dialog_content, null);
        scroll.addView(mContentView);
        setView(scroll);
        Resources resources = context.getResources();

        setButton(AlertDialog.BUTTON_NEGATIVE,
                resources.getString(R.string.autofill_negative_button), this);
        setButton(AlertDialog.BUTTON_POSITIVE,
                getContext().getResources().getString(R.string.autofill_positive_button), this);

        mContentView.setOnItemSelectedListener(this);

        AutofillDialogMenuItem[] billingItems = {
                new AutofillDialogMenuItem(ADD_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_new_billing)),
                new AutofillDialogMenuItem(EDIT_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_edit_billing))
        };
        AutofillDialogMenuItem[] shippingItems = {
                new AutofillDialogMenuItem(ADD_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_new_shipping)),
                new AutofillDialogMenuItem(EDIT_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_edit_shipping))
        };

        mDefaultMenuItems[AutofillDialogConstants.SECTION_CC_BILLING] = billingItems;
        mDefaultMenuItems[AutofillDialogConstants.SECTION_SHIPPING] = shippingItems;

        mDefaultAccountItems.add(resources.getString(R.string.autofill_new_account));
        mDefaultAccountItems.add(resources.getString(R.string.autofill_use_local));
    }

    @Override
    public void dismiss() {
        if (mWillDismiss) super.dismiss();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (!mContentView.isInEditingMode()) {
            mWillDismiss = true;
            if (which == AlertDialog.BUTTON_POSITIVE) mDelegate.dialogSubmit();
            else mDelegate.dialogCancel();
            return;
        }

        int section = mContentView.getCurrentSection();
        assert(section != AutofillDialogUtils.INVALID_SECTION);

        if (which == AlertDialog.BUTTON_POSITIVE) mDelegate.editingComplete(section);
        else mDelegate.editingCancel(section);

        mContentView.changeLayoutTo(AutofillDialogContentView.LAYOUT_STEADY);
        getButton(BUTTON_POSITIVE).setText(R.string.autofill_positive_button);
        mWillDismiss = false;
    }

    @Override
    public void onItemSelected(AdapterView<?> spinner, View view, int position, long id) {
        if (spinner.getId() == R.id.accounts_spinner) return;

        int section = AutofillDialogUtils.getSectionForSpinnerID(spinner.getId());

        if (!selectionShouldChangeLayout(spinner, section, position)) {
            mDelegate.itemSelected(section, position);
            return;
        }

        mDelegate.editingStart(section);
        AutofillDialogMenuItem currentItem =
                (AutofillDialogMenuItem) spinner.getItemAtPosition(position);
        if (currentItem.mIndex == ADD_MENU_ITEM_INDEX) clearAutofillSectionFieldValues(section);
        mContentView.changeLayoutTo(AutofillDialogContentView.getLayoutModeForSection(section));
        getButton(BUTTON_POSITIVE).setText(R.string.autofill_positive_button_editing);
    }

    @Override
    public void onNothingSelected(AdapterView<?> spinner) {
    }

    /**
     * @param spinner The dropdown that was selected by the user.
     * @param section The section  that the dropdown corresponds to.
     * @param position The position for the selected item in the dropdown.
     * @return Whether the selection should cause a layout state change.
     */
    private boolean selectionShouldChangeLayout(AdapterView<?> spinner, int section, int position) {
        int numDefaultItems = mDefaultMenuItems[section] != null ?
                mDefaultMenuItems[section].length : 0;
        return position >= spinner.getCount() - numDefaultItems;
    }

    //TODO(yusufo): Remove this. updateAccountChooserAndAddTitle will get initiated from glue.
    private void addTitleWithPlaceholders() {
       String[] accounts = {
               "placeholder@gmail.com",
               "placeholder@google.com"
       };
       updateAccountChooserAndAddTitle(accounts, 0);
    }

    /**
     * Update account chooser dropdown with given accounts and create the title view if needed.
     * @param accounts The accounts to be used for the dropdown.
     * @param selectedAccountIndex The index of a currently selected account or -1
     *                             if the local payments should be used.
     */
    public void updateAccountChooserAndAddTitle(String[] accounts, int selectedAccountIndex) {
        ArrayList<String> combinedItems =
                new ArrayList<String>(accounts.length + mDefaultAccountItems.size());
        combinedItems.addAll(Arrays.asList(accounts));
        combinedItems.addAll(mDefaultAccountItems);
        if (mTitleView == null) {
            mTitleView = new AutofillDialogTitleView(getContext(), combinedItems);
            mTitleView.setOnItemSelectedListener(this);
            setCustomTitle(mTitleView);
            return;
        }
        mTitleView.updateAccountsAndSelect(combinedItems, selectedAccountIndex);
    }

    /**
     * Notifies the dialog that the underlying model is changed and all sections will be updated.
     * Any editing should be invalidated.
     * The dialog should either go to the FETCHING or to the STEADY mode.
     * @param fetchingIsActive If true, the data is being fetched and is not yet available.
     */
    public void modelChanged(boolean fetchingIsActive) {
        if (fetchingIsActive) {
            mContentView.changeLayoutTo(AutofillDialogContentView.LAYOUT_FETCHING);
        } else {
            mContentView.changeLayoutTo(AutofillDialogContentView.LAYOUT_STEADY);
        }
    }

    /**
     * Update notification area with the provided notifications.
     * @param notifications Array of notifications to be displayed in the dialog.
     */
    public void updateNotificationArea(AutofillDialogNotification[] notifications) {
        for (AutofillDialogNotification notification: notifications) {
            if (notification.mHasArrow && notification.mHasCheckbox) {
                // Assuming that there will always be only one top notification.
                CheckBox checkBox = ((CheckBox) findViewById(R.id.top_notification));
                checkBox.setBackgroundColor(notification.mBackgroundColor);
                checkBox.setTextColor(notification.mTextColor);
                checkBox.setText(notification.mText);
                checkBox.setVisibility(View.VISIBLE);
            } else {
                TextView notificationView = new TextView(getContext());
                notificationView.setBackgroundColor(notification.mBackgroundColor);
                notificationView.setTextColor(notification.mTextColor);
                notificationView.setText(notification.mText);
                notificationView.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                        getContext().getResources().getDimension(
                                R.dimen.autofill_notification_text_size));
                int padding = getContext().getResources().
                        getDimensionPixelSize(R.dimen.autofill_notification_padding);
                notificationView.setPadding(padding, padding, padding, padding);

                ViewGroup notificationsContainer =
                        ((ViewGroup) findViewById(R.id.bottom_notifications));
                notificationsContainer.addView(notificationView);
                notificationsContainer.setVisibility(View.VISIBLE);
            }
        }
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
        List<AutofillDialogMenuItem> combinedItems;
        if (mDefaultMenuItems[section] == null) {
            combinedItems = Arrays.asList(menuItems);
        } else {
            combinedItems = new ArrayList<AutofillDialogMenuItem>(
                    menuItems.length + mDefaultMenuItems[section].length);
            combinedItems.addAll(Arrays.asList(menuItems));
            combinedItems.addAll(Arrays.asList(mDefaultMenuItems[section]));
        }
        mContentView.updateMenuItemsForSection(section, combinedItems);
        mAutofillSectionMenuData[section] = menuItems;
    }

    /**
     * Clears the field values for the given section.
     * @param section The section to clear the field values for.
     */
    public void clearAutofillSectionFieldData(int section) {
        AutofillDialogField[] fieldData = mAutofillSectionFieldData[section];
        if (fieldData == null) return;

        for (int i = 0; i < fieldData.length; i++) fieldData[i].setValue("");
    }

    private void clearAutofillSectionFieldValues(int section) {
        AutofillDialogField[] fieldData = mAutofillSectionFieldData[section];
        if (fieldData == null) return;

        EditText currentField;
        for (int i = 0; i < fieldData.length; i++) {
            currentField = (EditText) findViewById(AutofillDialogUtils.getEditTextIDForField(
                    section, fieldData[i].mFieldType));
            if (currentField == null) continue;

            currentField.setText("");
        }
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