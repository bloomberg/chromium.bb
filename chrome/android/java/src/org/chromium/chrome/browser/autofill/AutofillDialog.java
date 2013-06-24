// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.TypedValue;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.LinearLayout.LayoutParams;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.ViewAndroidDelegate;

import java.util.Arrays;

/**
 * This is the dialog that will act as the java side controller between the
 * AutofillDialogGlue object and the UI elements on the page. It contains the
 * title and the content views and relays messages from the backend to them.
 */
public class AutofillDialog extends Dialog
        implements OnClickListener, OnItemSelectedListener,
                AutofillDialogView.OnItemEditButtonClickedListener,
                OnFocusChangeListener, ViewAndroidDelegate {
    private final AutofillDialogView mView;
    private final AutofillDialogTitleView mTitleView;
    private final AutofillDialogDelegate mDelegate;

    private AutofillDialogField[][] mAutofillSectionFieldData =
            new AutofillDialogField[AutofillDialogConstants.NUM_SECTIONS][];
    private TextWatcher mCurrentTextWatcher;
    private int mFocusedFieldNativePointer;
    private EditText mFocusedField;

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
         * Called when a checkbox in the notification area has changed its state.
         * @param type Notification type.
         * @param checked Whether the checkbox is checked.
         */
        public void notificationCheckboxStateChanged(int type, boolean checked);

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
         * Informs AutofillDialog controller that the user has edited or activate an EditText field.
         * @param section Section that is being edited. Should match one of the values in
         *                {@link AutofillDialogConstants}.
         * @param dialogInputPointer The native pointer to the field that was edited or activated.
         * @param delegate The ViewAndroidDelegate that should be used to attach the AutofillPopup
         *                 if the AutofillPopup controller has any suggestions.
         * @param value The current value the focused field.
         * @param wasEdit True if it is an edit, otherwise False.
         */
        public void editedOrActivatedField(int section, int dialogInputPointer,
                ViewAndroidDelegate delegate, String  value, boolean wasEdit);

        /**
         * Requests AutofillDialog controller to validate the specified field. If the field is
         * invalid the returned value contains the error string. If the field is valid then
         * the returned value is null.
         * @param section Section that is being edited. Should match one of the values in
         *                {@link AutofillDialogConstants}.
         * @param fieldType The type of the field that is being edited. Should match one of the
         *                  values in {@link AutofillDialogConstants}.
         * @param value The value of the field that is being edited.
         * @return The error if the field is not valid, otherwise return null.
         */
        public String validateField(int section, int fieldType, String value);

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
         * Informs AutofillDialog controller that the user closed the dialog.
         */
        public void dialogDismissed();

        /**
         * Get the list associated with this field as a string array.
         * @param field The field for which the list should be returned.
         * @return A string array that contains the list
         **/
        public String[] getListForField(int field);

        /**
         * @param section Section for which the label should be returned.
         * @return The string that should appear on the label for the given section.
         */
        public String getLabelForSection(int section);

        /**
         * @param fieldType The field type to return the icon for.
         * @param input The current user input on the field.
         * @return The bitmap resource that should be shown on the field.
         */
        public Bitmap getIconForField(int fieldType, String input);

        /**
         * @param section The section associated with the field.
         * @param fieldType The field type to return the icon for.
         * @return The placeholder string that should be shown on the field.
         */
        public String getPlaceholderForField(int section, int fieldType);

        /**
         * @param dialogButtonId AutofillDialogConstants.DIALOG_BUTTON_ ID of the button.
         * @return The text for the given button.
         */
        public String getDialogButtonText(int dialogButtonId);

        /**
         * @param dialogButtonId AutofillDialogConstants.DIALOG_BUTTON_ ID of the button.
         * @return Whether the given button should be enabled.
         */
        public boolean isDialogButtonEnabled(int dialogButtonId);

        /**
         * @return The "Save locally" checkbox label.
         */
        public String getSaveLocallyText();

        /**
         * @return The legal documents text.
         */
        public String getLegalDocumentsText();

        /**
         * @param section The section.
         * @param position The index of an item.
         * @return Whether an item is the "Add..." item.
         */
        public boolean isTheAddItem(int section, int position);
    }

    protected AutofillDialog(Context context, AutofillDialogDelegate delegate) {
        super(context);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        mDelegate = delegate;
        mView = (AutofillDialogView) getLayoutInflater().
                inflate(R.layout.autofill_dialog_content, null);

        mTitleView = (AutofillDialogTitleView) mView.findViewById(R.id.title);
        mTitleView.setOnItemSelectedListener(this);

        getSaveLocallyCheckBox().setText(mDelegate.getSaveLocallyText());
        getSaveLocallyCheckBox().setChecked(true);

        String[] labels = new String[AutofillDialogConstants.NUM_SECTIONS];
        for (int i = 0; i < AutofillDialogConstants.NUM_SECTIONS; i++) {
            labels[i] = mDelegate.getLabelForSection(i);
        }
        getWindow().setLayout(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        setContentView(mView);
        mView.initialize(this);
        mView.initializeLabelsForEachSection(labels);

        setButton(AlertDialog.BUTTON_NEGATIVE,
                mDelegate.getDialogButtonText(AutofillDialogConstants.DIALOG_BUTTON_CANCEL), this);
        setButton(AlertDialog.BUTTON_POSITIVE,
                mDelegate.getDialogButtonText(AutofillDialogConstants.DIALOG_BUTTON_OK), this);

        mView.setOnItemSelectedListener(this);
        mView.setOnItemEditButtonClickedListener(this);
    }

    private AutofillDialogField[] getFieldsForSection(int section) {
        if (section < 0 || section >= mAutofillSectionFieldData.length) {
            assert false;
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
        // Any calls coming from the Android View system are ignored.
        // If the dialog should be dismissed, internalDismiss() should be used.
        // TODO(yusufo): http://crbug.com/234477 Consider not using AlertDialog.
    }

    @Override
    public void show() {
        mView.createAdapters();
        super.show();
    }

    /**
     * Dismisses the dialog.
     **/
    private void internalDismiss() {
        super.dismiss();
        mDelegate.dialogDismissed();
    }

    private void setButton(int which, String text, android.view.View.OnClickListener listener) {
        Button button = getButton(which);

        if (button == null) return;
        assert(button != null);

        button.setVisibility(View.VISIBLE);
        button.setText(text);
        button.setOnClickListener(listener);
    }

    private Button getButton(int which) {
        if (which == AlertDialog.BUTTON_POSITIVE) {
            return (Button) mView.findViewById(R.id.positive_button);
        } else {
            return (Button) mView.findViewById(R.id.negative_button);
        }
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
    public void onClick(View button) {
        int which;
        if (button.getId() == R.id.positive_button) {
            which = AlertDialog.BUTTON_POSITIVE;
        } else {
            which = AlertDialog.BUTTON_NEGATIVE;
        }

        // Note that the dialog will NOT be dismissed automatically.
        if (!mView.isInEditingMode()) {
            if (which == AlertDialog.BUTTON_POSITIVE) {
                // The controller will dismiss the dialog if the validation succeeds.
                // Otherwise, the dialog should be in the operational state to show
                // errors, challenges and notifications.
                mDelegate.dialogSubmit();
            } else {
                // The dialog will be dismissed with a call to dismissAutofillDialog().
                mDelegate.dialogCancel();
            }
            // The buttons will be updated as the result of a controller callback.
            disableButtons();
            return;
        }

        int section = mView.getCurrentSection();
        assert(section != AutofillDialogUtils.INVALID_SECTION);

        if (which == AlertDialog.BUTTON_POSITIVE) {
            // Switch the layout only if validation passes.
            if (!mDelegate.editingComplete(section)) return;
        } else {
            mDelegate.editingCancel(section);
        }
        changeLayoutTo(AutofillDialogView.LAYOUT_STEADY);
    }

    @Override
    public void onItemSelected(AdapterView<?> spinner, View view, int position, long id) {
        if (spinner.getId() == R.id.accounts_spinner) {
            mDelegate.accountSelected(position);
            return;
        }

        int section = AutofillDialogUtils.getSectionForSpinnerID(spinner.getId());
        if (!isTheAddItem(spinner, section, position)) {
            mDelegate.itemSelected(section, position);
            return;
        }

        onItemEditButtonClicked(section, position);
    }

    @Override
    public void onItemEditButtonClicked(int section, int position) {
        mView.updateMenuSelectionForSection(section, position);
        mDelegate.itemSelected(section, position);
        mDelegate.editingStart(section);

        changeLayoutTo(AutofillDialogView.getLayoutModeForSection(section));
    }

    @Override
    public void onNothingSelected(AdapterView<?> spinner) {
    }

    /**
     * @param spinner The dropdown that was selected by the user.
     * @param section The section  that the dropdown corresponds to.
     * @param position The position for the selected item in the dropdown.
     * @return Whether the selection is the "Add..." item.
     */
    private boolean isTheAddItem(AdapterView<?> spinner, int section, int position) {
        return mDelegate.isTheAddItem(section, position);
    }

    /**
     * Disables the dialog buttons.
     */
    private void disableButtons() {
        getButton(BUTTON_NEGATIVE).setEnabled(false);
        getButton(BUTTON_POSITIVE).setEnabled(false);
        mTitleView.setAccountChooserEnabled(false);
    }

    /**
     * Updates the buttons state for the given mode.
     * @param mode The layout mode.
     */
    private void updateButtons(int mode) {
        final Button negative = getButton(BUTTON_NEGATIVE);
        final Button positive = getButton(BUTTON_POSITIVE);

        switch (mode) {
            case AutofillDialogView.LAYOUT_FETCHING:
                negative.setText(mDelegate.getDialogButtonText(
                        AutofillDialogConstants.DIALOG_BUTTON_CANCEL));
                negative.setEnabled(mDelegate.isDialogButtonEnabled(
                        AutofillDialogConstants.DIALOG_BUTTON_CANCEL));
                positive.setText(mDelegate.getDialogButtonText(
                        AutofillDialogConstants.DIALOG_BUTTON_OK));
                positive.setEnabled(false);
                mTitleView.setAccountChooserEnabled(false);
                break;
            case AutofillDialogView.LAYOUT_STEADY:
                negative.setText(mDelegate.getDialogButtonText(
                        AutofillDialogConstants.DIALOG_BUTTON_CANCEL));
                negative.setEnabled(mDelegate.isDialogButtonEnabled(
                        AutofillDialogConstants.DIALOG_BUTTON_CANCEL));
                positive.setText(mDelegate.getDialogButtonText(
                        AutofillDialogConstants.DIALOG_BUTTON_OK));
                positive.setEnabled(mDelegate.isDialogButtonEnabled(
                        AutofillDialogConstants.DIALOG_BUTTON_OK));
                mTitleView.setAccountChooserEnabled(true);
                break;
            default:
                negative.setText(R.string.autofill_negative_button_editing);
                negative.setEnabled(true);
                positive.setText(R.string.autofill_positive_button_editing);
                positive.setEnabled(true);
                mTitleView.setAccountChooserEnabled(false);
                break;
        }
    }

    /**
     * Transitions the layout shown to a given layout.
     * @param mode The layout mode to transition to.
     */
    private void changeLayoutTo(int mode) {
        mView.changeLayoutTo(mode);
        updateButtons(mode);
        UiUtils.hideKeyboard(mView);
        if (mFocusedField != null && !mView.isInEditingMode()) {
            mFocusedField.removeTextChangedListener(mCurrentTextWatcher);
            mFocusedField = null;
        }
    }

    /**
     * Updates the account chooser dropdown with given accounts.
     * @param accounts The accounts to be used for the dropdown.
     * @param selectedAccountIndex The index of a currently selected account.
     */
    public void updateAccountChooser(String[] accounts, int selectedAccountIndex) {
        mTitleView.updateAccountsAndSelect(Arrays.asList(accounts), selectedAccountIndex);
        mView.updateLegalDocumentsText(mDelegate.getLegalDocumentsText());
    }

    /**
     * Notifies the dialog that the underlying model is changed and all sections will be updated.
     * Any editing should be invalidated.
     * The dialog should either go to the FETCHING or to the STEADY mode.
     * @param fetchingIsActive If true, the data is being fetched and is not yet available.
     */
    public void modelChanged(boolean fetchingIsActive) {
        if (fetchingIsActive) {
            changeLayoutTo(AutofillDialogView.LAYOUT_FETCHING);
            mTitleView.hideLogoAndAccountChooserVisibility();
        } else {
            changeLayoutTo(AutofillDialogView.LAYOUT_STEADY);
        }
    }

    /**
     * Update notification area with the provided notifications.
     * @param notifications Array of notifications to be displayed in the dialog.
     */
    public void updateNotificationArea(AutofillDialogNotification[] notifications) {
        // Clear all the previous notifications
        CheckBox checkBox = ((CheckBox) findViewById(R.id.top_checkbox_notification));
        checkBox.setOnCheckedChangeListener(null);
        checkBox.setVisibility(View.GONE);
        ViewGroup notificationsContainer =
                ((ViewGroup) findViewById(R.id.top_notifications));
        notificationsContainer.removeAllViews();

        // Add new notifications
        for (AutofillDialogNotification notification: notifications) {
            if (notification.mHasArrow && notification.mHasCheckbox) {
                // Assuming that there will always be only one top notification.
                checkBox.setBackgroundColor(notification.mBackgroundColor);
                checkBox.setTextColor(notification.mTextColor);
                checkBox.setText(notification.mText);
                checkBox.setEnabled(notification.mInteractive);
                checkBox.setChecked(notification.mChecked);
                checkBox.setVisibility(View.VISIBLE);
                final int type = notification.mType;
                checkBox.setOnCheckedChangeListener(new OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton view, boolean checked) {
                        mDelegate.notificationCheckboxStateChanged(type, checked);
                    }
                });
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
     * @param suggestionText The suggestion text.
     * @param suggestionIcon The suggestion icon.
     * @param suggestionTextExtra The suggestion text extra.
     * @param suggestionIconExtra The suggestion icon extra.
     * @param menuItems The array that contains the dropdown items to be shown for the section.
     * @param selectedMenuItem The menu item that is currently selected or -1 otherwise.
     * @param clobberInputs Whether to clobber the user input.
     * @param fieldTypeToAlwaysClobber Field type to be clobbered anyway, or UNKNOWN_TYPE.
     */
    public void updateSection(int section, boolean visible, AutofillDialogField[] dialogInputs,
            String suggestionText, Bitmap suggestionIcon, String suggestionTextExtra,
                    Bitmap suggestionIconExtra, AutofillDialogMenuItem[] menuItems,
                            int selectedMenuItem, boolean clobberInputs,
                                    int fieldTypeToAlwaysClobber) {
        View currentField;
        String inputValue;

        for (int i = 0; i < dialogInputs.length; i++) {
            currentField = findViewById(AutofillDialogUtils.getViewIDForField(
                    section, dialogInputs[i].mFieldType));
            if (currentField instanceof EditText) {
                EditText currentEdit = (EditText) currentField;
                if (!clobberInputs
                    && !TextUtils.isEmpty(currentEdit.getText())
                    && dialogInputs[i].mFieldType != fieldTypeToAlwaysClobber) {
                    continue;
                }

                if (AutofillDialogUtils.containsCreditCardInfo(section)
                        && dialogInputs[i].mFieldType
                                == AutofillDialogConstants.CREDIT_CARD_VERIFICATION_CODE) {
                    currentEdit.setCompoundDrawables(null, null,
                            mView.createFieldIconDrawable(suggestionIconExtra), null);
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
        mView.setVisibilityForSection(section, visible);

        updateSectionMenuItems(section,
                suggestionText, suggestionIcon, suggestionTextExtra, suggestionIconExtra,
                        menuItems, selectedMenuItem);
    }

    /**
     * Updates menu items in a given section with the data provided.
     * @param section The section to update with the given data.
     * @param suggestionText The suggestion text.
     * @param suggestionIcon The suggestion icon.
     * @param suggestionTextExtra The suggestion text extra.
     * @param suggestionIconExtra The suggestion icon extra.
     * @param menuItems The array that contains the dropdown items to be shown for the section.
     * @param selectedMenuItem The menu item that is currently selected or -1 otherwise.
     */
    public void updateSectionMenuItems(
            int section, String suggestionText, Bitmap suggestionIcon, String suggestionTextExtra,
                    Bitmap suggestionIconExtra, AutofillDialogMenuItem[] menuItems,
                            int selectedMenuItem) {
        mView.updateMenuItemsForSection(
                section, suggestionText, suggestionIcon, suggestionTextExtra, suggestionIconExtra,
                        Arrays.asList(menuItems), selectedMenuItem);
    }

    /**
     * Update validation error values for the given section.
     * @param section The section that needs to be updated.
     * @param errors The array of errors that were found when validating the fields.
     */
    public void updateSectionErrors(int section, AutofillDialogFieldError[] errors) {
        AutofillDialogField[] fields = getFieldsForSection(section);
        if (fields != null) {
            // Clear old errors.
            for (AutofillDialogField field : fields) {
                View currentField = findViewById(
                        AutofillDialogUtils.getViewIDForField(section, field.mFieldType));
                if (currentField instanceof EditText) {
                    ((EditText) currentField).setError(null);
                } else if (currentField instanceof Spinner) {
                    View innerView =
                            currentField.findViewById(R.id.autofill_editing_spinner_item);
                    if (innerView instanceof TextView) ((TextView) innerView).setError(null);
                }
            }
        }

        // Add new errors.
        for(AutofillDialogFieldError error : errors) {
            View currentField = findViewById(
                    AutofillDialogUtils.getViewIDForField(section, error.mFieldType));
            if (currentField instanceof EditText) {
                ((EditText) currentField).setError(error.mErrorText);
            } else if (currentField instanceof Spinner) {
                View innerView =
                        currentField.findViewById(R.id.autofill_editing_spinner_item);
                if (innerView instanceof TextView) {
                    ((TextView) innerView).setError(error.mErrorText);
                }
            }
        }
    }

    /**
     * Clears the field values for the given section.
     * @param section The section to clear the field values for.
     */
    public void clearAutofillSectionFieldData(int section) {
        AutofillDialogField[] fields = getFieldsForSection(section);
        if (fields == null) return;

        for (AutofillDialogField field : fields) field.setValue("");
    }

    private void clearAutofillSectionFieldValues(int section) {
        AutofillDialogField[] fields = getFieldsForSection(section);
        if (fields == null) return;

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

    private CheckBox getSaveLocallyCheckBox() {
        return (CheckBox) mView.findViewById(R.id.save_locally_checkbox);
    }

    /**
     * Update the visibility for the save locally checkbox.
     * @param shouldShow Whether the checkbox should be shown or hidden.
     */
    public void updateSaveLocallyCheckBox(boolean shouldShow) {
        getSaveLocallyCheckBox().setVisibility(
                shouldShow ? View.VISIBLE : View.GONE);
    }

    /**
     * Return the array that holds all the data about the fields in the given section.
     * @param section The section to return the data for.
     * @return An array containing the data for each field in the given section.
     */
    public AutofillDialogField[] getSection(int section) {
        AutofillDialogField[] fields = getFieldsForSection(section);
        if (fields == null) return null;

        for (AutofillDialogField field : fields) {
            View currentField = findViewById(
                    AutofillDialogUtils.getViewIDForField(section, field.mFieldType));
            if (currentField == null) continue;
            String currentValue = "";
            if (currentField instanceof EditText)
                currentValue = ((EditText) currentField).getText().toString();
            else if (currentField instanceof Spinner) {
                Object selectedItem = ((Spinner) currentField).getSelectedItem();
                currentValue = selectedItem != null ? selectedItem.toString() : "";
            }
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
     * @return Whether the details entered should be saved locally on the device.
     */
    public boolean shouldSaveDetailsLocally() {
        CheckBox saveLocallyCheckBox = getSaveLocallyCheckBox();
        return saveLocallyCheckBox.isShown() && saveLocallyCheckBox.isChecked();
    }

    /**
     * Updates the progress for the final transaction with the given value.
     * @param value The current progress percentage value.
     */
    public void updateProgressBar(double value) {
    }

    /**
     * Dismisses the Autofill dialog as if cancel was pressed.
     */
    public void dismissAutofillDialog() {
        internalDismiss();
    }

    private void addTextWatcher(EditText view, int nativePointer) {
        mFocusedFieldNativePointer = nativePointer;
        if (mCurrentTextWatcher == null) {
            mCurrentTextWatcher = new TextWatcher() {
                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {
                }

                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                }

                @Override
                public void afterTextChanged(Editable editable) {
                    mDelegate.editedOrActivatedField(mView.getCurrentSection(),
                            mFocusedFieldNativePointer, AutofillDialog.this, editable.toString(),
                            true);
                }
            };
        }
        view.addTextChangedListener(mCurrentTextWatcher);
    }

    /**
     * Follow the EditText that is currently focused and add/remove text watcher for that EditText.
     * Fields also get validated when one of them is defocused.
     * @param v View that just got a focus change.
     * @param hasFocus Whether the focus was gained.
     */
    @Override
    public void onFocusChange(View v, boolean hasFocus) {
        if (!mView.isInEditingMode()) return;

        if (!(v instanceof EditText)) return;
        EditText currentfield = (EditText) v;

        // New EditText just got focused.
        if (hasFocus) mFocusedField = currentfield;

        int section = mView.getCurrentSection();
        AutofillDialogField[] fields = getFieldsForSection(section);
        int fieldType = AutofillDialogConstants.UNKNOWN_TYPE;
        int nativePointer = 0;
        for (AutofillDialogField field : fields) {
            View currentView = findViewById(AutofillDialogUtils.getViewIDForField(
                    section, field.mFieldType));
            if (v.equals(currentView)) {
                fieldType = field.mFieldType;
                nativePointer = field.mNativePointer;
                break;
            }
        }
        assert (fieldType != AutofillDialogConstants.UNKNOWN_TYPE);
        if (fieldType == AutofillDialogConstants.UNKNOWN_TYPE) return;

        if (hasFocus) {
            // Add text watcher to the currently selected EditText and send out the first
            // editedOrActivated call because text watcher only sends it out after text changes.
            addTextWatcher(currentfield, nativePointer);
            mDelegate.editedOrActivatedField(mView.getCurrentSection(), mFocusedFieldNativePointer,
                    this, mFocusedField.getText().toString(), false);
        } else {
            // Remove text watcher for the EditText that is being defocused, then validate that
            // field. Entire section is validated if the field is valid.
            currentfield.removeTextChangedListener(mCurrentTextWatcher);
            String errorText = mDelegate.validateField(mView.getCurrentSection(), fieldType,
                    currentfield.getText().toString());
            currentfield.setError(errorText);
            if (errorText == null) mDelegate.validateSection(section);
        }
    }

    @Override
    public View acquireAnchorView() {
        return mFocusedField;
    }

    @Override
    public void setAnchorViewPosition(View view, float x, float y, float width, float height) {
    }

    @Override
    public void releaseAnchorView(View anchorView) {
    }
}
