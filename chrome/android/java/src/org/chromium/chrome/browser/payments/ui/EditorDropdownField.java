// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.autofill.AutofillProfileBridge.DropdownKeyValue;

import java.util.List;

/**
 * Helper class for creating a dropdown view with a label.
 */
class EditorDropdownField {
    private final EditorFieldModel mFieldModel;
    private final View mLayout;
    private final TextView mLabel;
    private final Spinner mDropdown;
    private int mSelectedIndex;

    /**
     * Builds a dropdown view.
     *
     * @param context         The application context to use when creating widgets.
     * @param fieldModel      The data model of the dropdown.
     * @param changedCallback The callback to invoke after user's dropdwn item selection has been
     *                        processed.
     */
    public EditorDropdownField(Context context, final EditorFieldModel fieldModel,
            final Runnable changedCallback) {
        assert fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN;
        mFieldModel = fieldModel;

        mLayout = LayoutInflater.from(context).inflate(
                R.layout.payment_request_editor_dropdown, null, false);

        mLabel = (TextView) mLayout.findViewById(R.id.spinner_label);
        mLabel.setText(mFieldModel.getLabel());

        final List<DropdownKeyValue> dropdownKeyValues = mFieldModel.getDropdownKeyValues();
        for (int j = 0; j < dropdownKeyValues.size(); j++) {
            if (dropdownKeyValues.get(j).getKey().equals(mFieldModel.getValue())) {
                mSelectedIndex = j;
                break;
            }
        }

        ArrayAdapter<DropdownKeyValue> adapter = new ArrayAdapter<DropdownKeyValue>(
                context, android.R.layout.simple_spinner_item, dropdownKeyValues);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

        mDropdown = (Spinner) mLayout.findViewById(R.id.spinner);
        mDropdown.setContentDescription(mFieldModel.getLabel());
        mDropdown.setAdapter(adapter);
        mDropdown.setSelection(mSelectedIndex);
        mDropdown.setOnItemSelectedListener(new OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (mSelectedIndex != position) {
                    mSelectedIndex = position;
                    mFieldModel.setDropdownKey(dropdownKeyValues.get(position).getKey(),
                            changedCallback);
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
    }

    /** @return The View containing everything. */
    public View getLayout() {
        return mLayout;
    }

    /** @return The EditorFieldModel that the EditorDropdownField represents. */
    public EditorFieldModel getFieldModel() {
        return mFieldModel;
    }

    /** @return The label view for the spinner. */
    public View getLabel() {
        return mLabel;
    }

    /** @return The dropdown view itself. */
    public View getDropdown() {
        return mDropdown;
    }
}
