// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.app.Fragment;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;

import com.android.i18n.addressinput.AddressData;
import com.android.i18n.addressinput.AddressField;
import com.android.i18n.addressinput.AddressWidget;
import com.android.i18n.addressinput.FormOptions;
import com.android.i18n.addressinput.SimpleClientCacheManager;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;

/**
 * Provides the Java-ui for editing a Profile autofill entry.
 */
public class AutofillProfileEditor extends Fragment implements TextWatcher,
        OnItemSelectedListener {
    // GUID of the profile we are editing.
    // May be the empty string if creating a new profile.
    private String mGUID;

    private AddressWidget mAddressWidget;
    private boolean mNoCountryItemIsSelected;
    private EditText mPhoneText;
    private EditText mEmailText;
    private String mLanguageCodeString;

    @Override
    public void onCreate(Bundle savedState) {
        super.onCreate(savedState);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        View v = inflater.inflate(R.layout.autofill_profile_editor, container, false);

        mPhoneText = (EditText) v.findViewById(R.id.autofill_profile_editor_phone_number_edit);
        mEmailText = (EditText) v.findViewById(R.id.autofill_profile_editor_email_address_edit);

        // We know which profile to edit based on the GUID stuffed in
        // our extras by AutofillPreferences.
        Bundle extras = getArguments();
        if (extras != null) {
            mGUID = extras.getString(AutofillPreferences.AUTOFILL_GUID);
        }
        if (mGUID == null) {
            mGUID = "";
            getActivity().setTitle(R.string.autofill_create_profile);
        } else {
            getActivity().setTitle(R.string.autofill_edit_profile);
        }

        addProfileDataToEditFields(v);
        hookupSaveCancelDeleteButtons(v);
        return v;
    }

    @Override
    public void afterTextChanged(Editable s) {}

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        enableSaveButton();
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        mAddressWidget.onItemSelected(parent, view, position, id);

        if (mNoCountryItemIsSelected) {
            mNoCountryItemIsSelected = false;
            return;
        }

        enableSaveButton();
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {}

    private void addProfileDataToEditFields(View v) {
        AddressData.Builder address = new AddressData.Builder();
        AutofillProfile profile = PersonalDataManager.getInstance().getProfile(mGUID);

        if (profile != null) {
            mPhoneText.setText(profile.getPhoneNumber());
            mEmailText.setText(profile.getEmailAddress());
            mLanguageCodeString = profile.getLanguageCode();

            address.set(AddressField.ADMIN_AREA, profile.getRegion());
            address.set(AddressField.LOCALITY, profile.getLocality());
            address.set(AddressField.RECIPIENT, profile.getFullName());
            address.set(AddressField.ORGANIZATION, profile.getCompanyName());
            address.set(AddressField.DEPENDENT_LOCALITY, profile.getDependentLocality());
            address.set(AddressField.POSTAL_CODE, profile.getPostalCode());
            address.set(AddressField.SORTING_CODE, profile.getSortingCode());
            address.set(AddressField.ADDRESS_LINE_1, profile.getStreetAddress());
            address.set(AddressField.COUNTRY, profile.getCountryCode());
        }

        ViewGroup widgetRoot = (ViewGroup) v.findViewById(R.id.autofill_profile_widget_root);
        mAddressWidget = new AddressWidget(getActivity(), widgetRoot,
                (new FormOptions.Builder()).build(),
                new SimpleClientCacheManager(),
                address.build(),
                new ChromeAddressWidgetUiComponentProvider(getActivity()));

        if (profile == null) {
            widgetRoot.requestFocus();
        }
    }

    // Read edited data; save in the associated Chrome profile.
    // Ignore empty fields.
    private void saveProfile() {
        AddressData input = mAddressWidget.getAddressData();
        String addressLines = input.getAddressLine1();
        if (!TextUtils.isEmpty(input.getAddressLine2())) {
            addressLines += "\n" + input.getAddressLine2();
        }

        AutofillProfile profile = new PersonalDataManager.AutofillProfile(
                mGUID,
                AutofillPreferences.SETTINGS_ORIGIN,
                input.getRecipient(),
                input.getOrganization(),
                addressLines,
                input.getAdministrativeArea(),
                input.getLocality(),
                input.getDependentLocality(),
                input.getPostalCode(),
                input.getSortingCode(),
                input.getPostalCountry(),
                mPhoneText.getText().toString(),
                mEmailText.getText().toString(),
                mLanguageCodeString);
        PersonalDataManager.getInstance().setProfile(profile);
    }

    private void deleteProfile() {
        if (AutofillProfileEditor.this.mGUID != null) {
            PersonalDataManager.getInstance().deleteProfile(mGUID);
        }
    }

    private void hookupSaveCancelDeleteButtons(View v) {
        Button button = (Button) v.findViewById(R.id.autofill_profile_delete);
        if ((mGUID == null) || (mGUID.compareTo("") == 0)) {
            // If this is a create, disable the delete button.
            button.setEnabled(false);
        } else {
            button.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        AutofillProfileEditor.this.deleteProfile();
                        getActivity().finish();
                    }
                });
        }
        button = (Button) v.findViewById(R.id.autofill_profile_cancel);
        button.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    getActivity().finish();
                }
            });
        button = (Button) v.findViewById(R.id.autofill_profile_save);
        button.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    AutofillProfileEditor.this.saveProfile();
                    getActivity().finish();
                }
            });
        button.setEnabled(false);

        // Listen for changes to inputs. Enable the save button after something has changed.
        mPhoneText.addTextChangedListener(this);
        mEmailText.addTextChangedListener(this);
        mNoCountryItemIsSelected = true;

        for (AddressField field : AddressField.values()) {
            View input = mAddressWidget.getViewForField(field);
            if (input instanceof EditText) {
                ((EditText) input).addTextChangedListener(this);
            } else if (input instanceof Spinner) {
                ((Spinner) input).setOnItemSelectedListener(this);
            }
        }
    }

    private void enableSaveButton() {
        Button button = (Button) getView().findViewById(R.id.autofill_profile_save);
        button.setEnabled(true);
    }
}
