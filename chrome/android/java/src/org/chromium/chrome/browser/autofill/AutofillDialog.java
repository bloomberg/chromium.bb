// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;

import android.content.DialogInterface.OnClickListener;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.View;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.Spinner;
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
        implements OnClickListener, OnItemSelectedListener, OnFocusChangeListener {
    private static final int ADD_MENU_ITEM_INDEX = -1;
    private static final int EDIT_MENU_ITEM_INDEX = -2;

    private final AutofillDialogContentView mContentView;
    private final AutofillDialogTitleView mTitleView;
    private final AutofillDialogDelegate mDelegate;

    private AutofillDialogField[][] mAutofillSectionFieldData =
            new AutofillDialogField[AutofillDialogConstants.NUM_SECTIONS][];
    private AutofillDialogMenuItem[][] mAutofillSectionMenuData =
            new AutofillDialogMenuItem[AutofillDialogConstants.NUM_SECTIONS][];
    private final AutofillDialogMenuItem[][] mDefaultMenuItems =
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
         * Informs AutofillDialog controller that the view has completed editing and triggers
         * field validation that will set errors on all invalid fields.
         * @param section Section that was being edited. Should match one of the values in
         *                {@link AutofillDialogConstants}.
         * @return Whether editing is done and fields have no validation errors.
         */
        public boolean editingComplete(int section);

        /**
         * Informs AutofillDialog controller that the view has cancelled editing.
         * @param section Section that was being edited. Should match one of the values in
         *                {@link AutofillDialogConstants}.
         */
        public void editingCancel(int section);

        /**
         * Requests AutofillDialog controller to validate the specified field. If the field is
         * invalid the returned value contains the error string. If the field is valid then
         * the returned value is null.
         * @param fieldType The type of the field that is being edited. Should match one of the
         *                  values in {@link AutofillDialogConstants}.
         * @param value The value of the field that is being edited.
         * @return The error if the field is not valid, otherwise return null.
         */
        public String validateField(int fieldType, String value);

        /**
         * Requests AutofillDialog controller to validate the specified section.
         * If there are invalid fields then the controller will call updateSectionErrors().
         * @param section Section that is being edited. Should match one of the values in
         *                {@link AutofillDialogConstants}.
         */
        public void validateSection(int section);

        /**
         * Informs AutofillDialog controller that the user clicked on the submit button.
         */
        public void dialogSubmit();

        /**
         * Informs AutofillDialog controller that the user clicked on the cancel button.
         */
        public void dialogCancel();

        /**
         * Get the list associated with this field as a string array.
         * @param field The field for which the list should be returned.
         * @return A string array that contains the list
         **/
        public String[] getListForField(int field);

        /**
         * Returns the label string to be used for the given section.
         * @param section Section for which the label should be returned.
         * @return The string that should appear on the label for the given section.
         */
        public String getLabelForSection(int section);

        /**
         * Returns the bitmap icon associated with the given field.
         * @param fieldType The field type to return the icon for.
         * @param input The current user input on the field.
         * @return The bitmap resource that should be shown on the field.
         */
        public Bitmap getIconForField(int fieldType, String input);

        /**
         * Returns the placeholder string associated with the given field.
         * @param section The section associated with the field.
         * @param fieldType The field type to return the icon for.
         * @return The placeholder string that should be shown on the field.
         */
        public String getPlaceholderForField(int section, int fieldType);
    }

    protected AutofillDialog(Context context, AutofillDialogDelegate delegate) {
        super(context);
        mDelegate = delegate;

        mTitleView = new AutofillDialogTitleView(getContext());
        mTitleView.setOnItemSelectedListener(this);
        setCustomTitle(mTitleView);

        ScrollView scroll = new ScrollView(context);
        mContentView = (AutofillDialogContentView) getLayoutInflater().
                inflate(R.layout.autofill_dialog_content, null);
        mContentView.setAutofillDialog(this);
        String[] labels = new String[AutofillDialogConstants.NUM_SECTIONS];
        for (int i = 0; i < AutofillDialogConstants.NUM_SECTIONS; i++) {
            labels[i] = mDelegate.getLabelForSection(i);
        }
        mContentView.initializeLabelsForEachSection(labels);
        scroll.addView(mContentView);
        setView(scroll);
        Resources resources = context.getResources();

        setButton(AlertDialog.BUTTON_NEGATIVE,
                resources.getString(R.string.autofill_negative_button), this);
        setButton(AlertDialog.BUTTON_POSITIVE,
                getContext().getResources().getString(R.string.autofill_positive_button), this);

        mContentView.setOnItemSelectedListener(this);

        // TODO(aruslan): remove as part of the native model wrapper http://crbug.com/224162.
        final AutofillDialogMenuItem[] ccItems = {
                new AutofillDialogMenuItem(ADD_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_new_credit_card)),
                new AutofillDialogMenuItem(EDIT_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_edit_credit_card))
        };
        final AutofillDialogMenuItem[] billingDetailsItems = {
                new AutofillDialogMenuItem(ADD_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_new_cc_billing)),
                new AutofillDialogMenuItem(EDIT_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_edit_cc_billing))
        };
        final AutofillDialogMenuItem[] billingAddressItems = {
                new AutofillDialogMenuItem(ADD_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_new_billing)),
                new AutofillDialogMenuItem(EDIT_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_edit_billing))
        };
        final AutofillDialogMenuItem[] shippingItems = {
                new AutofillDialogMenuItem(ADD_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_new_shipping)),
                new AutofillDialogMenuItem(EDIT_MENU_ITEM_INDEX,
                        resources.getString(R.string.autofill_edit_shipping))
        };

        // TODO(yusufo): http://crbug.com/226497 Need an email add/edit layout/something.
        mDefaultMenuItems[AutofillDialogConstants.SECTION_CC] = ccItems;
        mDefaultMenuItems[AutofillDialogConstants.SECTION_BILLING] = billingAddressItems;
        mDefaultMenuItems[AutofillDialogConstants.SECTION_CC_BILLING] = billingDetailsItems;
        mDefaultMenuItems[AutofillDialogConstants.SECTION_SHIPPING] = shippingItems;

        String hint = mDelegate.getPlaceholderForField(
                AutofillDialogConstants.SECTION_CC,
                AutofillDialogConstants.CREDIT_CARD_VERIFICATION_CODE);
        Bitmap icon = mDelegate.getIconForField(
                AutofillDialogConstants.CREDIT_CARD_VERIFICATION_CODE, "");
        mContentView.setCVCInfo(hint, icon);
    }

    private AutofillDialogField[] getFieldsForSection(int section) {
        if (section < 0 || section >= mAutofillSectionFieldData.length) {
            assert(false);
            return new AutofillDialogField[0];
        }
        return mAutofillSectionFieldData[section];
    }

    private void setFieldsForSection(int section, AutofillDialogField[] fields) {
        if (section < 0 || section >= mAutofillSectionFieldData.length) return;
        mAutofillSectionFieldData[section] = fields;
    }

    @Override
    public void dismiss() {
        if (mWillDismiss) super.dismiss();
    }

    @Override
    public void show() {
        mContentView.createAdapters();
        super.show();
    }

    /**
     * Get the list associated with this field as a string array.
     * @param field The field for which the list should be returned.
     * @return A string array that contains the list
     **/
    public String[] getListForField(int field) {
        return mDelegate.getListForField(field);
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

        if (which == AlertDialog.BUTTON_POSITIVE) {
            // Switch the layout only if validation passes.
            if (!mDelegate.editingComplete(section)) return;
        } else {
            mDelegate.editingCancel(section);
        }
        mContentView.changeLayoutTo(AutofillDialogContentView.LAYOUT_STEADY);
        getButton(BUTTON_POSITIVE).setText(R.string.autofill_positive_button);
        mWillDismiss = false;
    }

    @Override
    public void onItemSelected(AdapterView<?> spinner, View view, int position, long id) {
        if (spinner.getId() == R.id.accounts_spinner) {
            mDelegate.accountSelected(position);
            return;
        }

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

    /**
     * Update account chooser dropdown with given accounts.
     * @param accounts The accounts to be used for the dropdown.
     * @param selectedAccountIndex The index of a currently selected account.
     */
    public void updateAccountChooser(String[] accounts, int selectedAccountIndex) {
        ArrayList<String> combinedItems = new ArrayList<String>(accounts.length);
        combinedItems.addAll(Arrays.asList(accounts));
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
        // Clear all the previous notifications
        CheckBox checkBox = ((CheckBox) findViewById(R.id.top_notification));
        checkBox.setVisibility(View.GONE);
        ViewGroup notificationsContainer =
                ((ViewGroup) findViewById(R.id.bottom_notifications));
        notificationsContainer.removeAllViews();

        // Add new notifications
        for (AutofillDialogNotification notification: notifications) {
            if (notification.mHasArrow && notification.mHasCheckbox) {
                // Assuming that there will always be only one top notification.
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
        View currentField;
        String inputValue;
        for (int i = 0; i < dialogInputs.length; i++) {
            currentField = findViewById(AutofillDialogUtils.getViewIDForField(
                    section, dialogInputs[i].mFieldType));
            if (currentField instanceof EditText) {
                EditText currentEdit = (EditText) currentField;
                if (AutofillDialogUtils.containsCreditCardInfo(section)
                        && dialogInputs[i].mFieldType
                                == AutofillDialogConstants.CREDIT_CARD_VERIFICATION_CODE) {
                    currentEdit.setCompoundDrawables(
                            null, null, mContentView.getCVCDrawable(), null);
                }
                currentEdit.setHint(dialogInputs[i].mPlaceholder);
                currentField.setOnFocusChangeListener(this);
                inputValue = dialogInputs[i].getValue();
                if (TextUtils.isEmpty(inputValue)) {
                    currentEdit.setText("");
                } else {
                    currentEdit.setText(inputValue);
                }
            } else if (currentField instanceof Spinner) {
                Spinner currentSpinner = (Spinner) currentField;
                for (int k = 0; k < currentSpinner.getCount(); k++) {
                    if (currentSpinner.getItemAtPosition(k).equals(dialogInputs[i]
                            .getValue())) {
                        currentSpinner.setSelection(k);
                    }
                    currentSpinner.setPrompt(dialogInputs[i].mPlaceholder);
                }
            }
        }
        setFieldsForSection(section, dialogInputs);
        mContentView.setVisibilityForSection(section, visible);
        List<AutofillDialogMenuItem> combinedItems;

        // This is a temporary hack in absence of an Android-specific model wrapper,
        // which would handle Edit/Add/EditingDone operations.
        // TODO(aruslan): remove as part of the native model wrapper http://crbug.com/224162.
        if (mDefaultMenuItems[section] == null || menuItems.length == 0) {
            combinedItems = Arrays.asList(menuItems);
        } else {
            combinedItems = new ArrayList<AutofillDialogMenuItem>(
                    menuItems.length - 1 + mDefaultMenuItems[section].length);
            combinedItems.addAll(Arrays.asList(menuItems));
            // Replace the provided "Add... item with ours and add "Edit".
            combinedItems.remove(menuItems.length - 1);
            combinedItems.addAll(Arrays.asList(mDefaultMenuItems[section]));
        }

        mContentView.updateMenuItemsForSection(section, combinedItems);
        mAutofillSectionMenuData[section] = menuItems;
    }

    /**
     * Update validation error values for the given section.
     * @param section The section that needs to be updated.
     * @param errors The array of errors that were found when validating the fields.
     */
    public void updateSectionErrors(int section, AutofillDialogFieldError[] errors) {
        AutofillDialogField[] fields = getFieldsForSection(section);
        // Clear old errors.
        for (AutofillDialogField field : fields) {
            View currentField = findViewById(
                    AutofillDialogUtils.getViewIDForField(section, field.mFieldType));
            if (currentField instanceof EditText)
                ((EditText) currentField).setError(null);
        }

        // Add new errors.
        for(AutofillDialogFieldError error : errors) {
            View currentField = findViewById(
                    AutofillDialogUtils.getViewIDForField(section, error.mFieldType));
            if (currentField instanceof EditText)
                ((EditText) currentField).setError(error.mErrorText);
        }
    }

    /**
     * Clears the field values for the given section.
     * @param section The section to clear the field values for.
     */
    public void clearAutofillSectionFieldData(int section) {
        AutofillDialogField[] fields = getFieldsForSection(section);
        for (AutofillDialogField field : fields) field.setValue("");
    }

    private void clearAutofillSectionFieldValues(int section) {
        AutofillDialogField[] fields = getFieldsForSection(section);
        for (AutofillDialogField field : fields) {
            View currentField = findViewById(
                    AutofillDialogUtils.getViewIDForField(section, field.mFieldType));
            // TODO(yusufo) remove this check when all the fields have been added.
            if (currentField instanceof EditText) {
                ((EditText) currentField).setText("");
            } else if (currentField instanceof Spinner) {
                ((Spinner) currentField).setSelected(false);
            }
        }
    }

    /**
     * Return the array that holds all the data about the fields in the given section.
     * @param section The section to return the data for.
     * @return An array containing the data for each field in the given section.
     */
    public AutofillDialogField[] getSection(int section) {
        AutofillDialogField[] fields = getFieldsForSection(section);
        for (AutofillDialogField field : fields) {
            View currentField = findViewById(
                    AutofillDialogUtils.getViewIDForField(section, field.mFieldType));
            if (currentField == null) continue;
            String currentValue = "";
            if (currentField instanceof EditText)
                currentValue = ((EditText) currentField).getText().toString();
            else if (currentField instanceof Spinner)
                currentValue = ((Spinner) currentField).getSelectedItem().toString();
            field.setValue(currentValue);
        }
        return fields;
    }

     /**
      * @return The currently entered or previously saved CVC value in the dialog.
      */
    public String getCvc() {
        EditText cvcEdit = (EditText) findViewById(R.id.cvc_challenge);
        if (cvcEdit != null) return cvcEdit.getText().toString();
        return "";
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

    /**
     * Validates EditText fields in the editing mode when they get unfocused.
     */
    @Override
    public void onFocusChange(View v, boolean hasFocus) {
        if (hasFocus || !mContentView.isInEditingMode()) return;

        if (!(v instanceof EditText)) return;
        EditText currentfield = (EditText) v;

        // Validation is performed when user changes from one EditText view to another.
        int section = mContentView.getCurrentSection();
        AutofillDialogField[] fields = getFieldsForSection(section);

        int fieldType = AutofillDialogConstants.UNKNOWN_TYPE;
        for (AutofillDialogField field : fields) {
            View currentView = findViewById(AutofillDialogUtils.getViewIDForField(
                    section, field.mFieldType));
            if (v.equals(currentView)) {
                fieldType = field.mFieldType;
                break;
            }
        }
        assert (fieldType != AutofillDialogConstants.UNKNOWN_TYPE);
        if (fieldType == AutofillDialogConstants.UNKNOWN_TYPE) return;

        String errorText = mDelegate.validateField(fieldType, currentfield.getText().toString());
        currentfield.setError(errorText);
        // Entire section is validated if the field is valid.
        if (errorText == null) mDelegate.validateSection(section);
    }
}
