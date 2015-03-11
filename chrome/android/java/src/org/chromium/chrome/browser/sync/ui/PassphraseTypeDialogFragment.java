// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.CheckedTextView;
import android.widget.ListView;


import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.sync.internal_api.pub.PassphraseType;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Set;

/**
 * Dialog to ask the user select what type of password to use for encryption.
 */
public class PassphraseTypeDialogFragment extends DialogFragment implements
        DialogInterface.OnClickListener, OnItemClickListener {

    interface Listener {
        void onPassphraseTypeSelected(PassphraseType type);
    }

    private static class PassphraseTypeUiElement {
        private final PassphraseType mPassphraseType;
        private final String mDisplayName;
        private int mPosition;

        private PassphraseTypeUiElement(PassphraseType passphraseType, String displayName) {
            mPassphraseType = passphraseType;
            mDisplayName = displayName;
            // Default position to an invalid value.
            mPosition = -1;
        }
    }

    private static class PassphraseTypeUiElementContainer {
        private final ArrayList<PassphraseTypeUiElement> mElements =
                new ArrayList<PassphraseTypeUiElement>();

        public void add(PassphraseTypeUiElement element) {
            // Now that we know where we will insert the element, we can update its position member.
            element.mPosition = mElements.size();
            mElements.add(element);
        }

        public ArrayList<PassphraseTypeUiElement> getElements() {
            return mElements;
        }

        public String[] getDisplayNames() {
            String[] strings = new String[mElements.size()];
            for (int i = 0; i < mElements.size(); i++) {
                strings[i] = mElements.get(i).mDisplayName;
            }
            return strings;
        }
    }

    private String textForPassphraseType(PassphraseType type) {
        switch (type) {
            case IMPLICIT_PASSPHRASE:  // Intentional fall through.
            case KEYSTORE_PASSPHRASE:
                return getString(R.string.sync_passphrase_type_keystore);
            case FROZEN_IMPLICIT_PASSPHRASE:
                String passphraseDate = getPassphraseDateStringFromArguments();
                String frozenPassphraseString = getString(R.string.sync_passphrase_type_frozen);
                return String.format(frozenPassphraseString, passphraseDate);
            case CUSTOM_PASSPHRASE:
                return getString(R.string.sync_passphrase_type_custom);
            default:
                return "";
        }
    }

    private Adapter createAdapter(PassphraseType currentType) {
        PassphraseTypeUiElementContainer container = new PassphraseTypeUiElementContainer();
        for (PassphraseType type : currentType.getVisibleTypes()) {
            container.add(new PassphraseTypeUiElement(type, textForPassphraseType(type)));
        }
        return new Adapter(container, container.getDisplayNames());
    }

    private class Adapter extends ArrayAdapter<String> {

        private final PassphraseTypeUiElementContainer mElementsContainer;

        /**
         * Do not call this constructor directly. Instead use
         * {@link PassphraseTypeDialogFragment#createAdapter}.
         */
        private Adapter(PassphraseTypeUiElementContainer elementsContainer,
                        String[] elementStrings) {
            super(getActivity(), R.layout.passphrase_type_item, elementStrings);
            mElementsContainer = elementsContainer;
        }

        @Override
        public boolean hasStableIds() {
            return true;
        }

        @Override
        public long getItemId(int position) {
            return getType(position).internalValue();
        }

        public PassphraseType getType(int position) {
            return mElementsContainer.getElements().get(position).mPassphraseType;
        }

        public int getPositionForType(PassphraseType type) {
            for (PassphraseTypeUiElement element : mElementsContainer.getElements()) {
                if (element.mPassphraseType == type) {
                    return element.mPosition;
                }
            }
            return -1;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            CheckedTextView view = (CheckedTextView) super.getView(position, convertView, parent);
            PassphraseType positionType = getType(position);
            PassphraseType currentType = getCurrentTypeFromArguments();
            Set<PassphraseType> allowedTypes = currentType.getAllowedTypes(
                    getEncryptEverythingAllowedFromArguments());

            // Set the item to checked it if it is the currently selected encryption type.
            view.setChecked(positionType == currentType);
            // Allow user to click on enabled types for the current type.
            view.setEnabled(allowedTypes.contains(positionType));
            return view;
        }
    }

    /**
     * This argument should contain a single value of type {@link PassphraseType}.
     */
    private static final String ARG_CURRENT_TYPE = "arg_current_type";

    private static final String ARG_PASSPHRASE_TIME = "arg_passphrase_time";

    private static final String ARG_ENCRYPT_EVERYTHING_ALLOWED = "arg_encrypt_everything_allowed";

    static PassphraseTypeDialogFragment create(PassphraseType currentType,
                                               long passphraseTime,
                                               boolean encryptEverythingAllowed) {
        PassphraseTypeDialogFragment dialog = new PassphraseTypeDialogFragment();
        Bundle args = new Bundle();
        args.putParcelable(ARG_CURRENT_TYPE, currentType);
        args.putLong(ARG_PASSPHRASE_TIME, passphraseTime);
        args.putBoolean(ARG_ENCRYPT_EVERYTHING_ALLOWED, encryptEverythingAllowed);
        dialog.setArguments(args);
        return dialog;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        // Configure the passphrase type list
        ListView list = new ListView(getActivity());
        Adapter adapter = createAdapter(getCurrentTypeFromArguments());
        list.setAdapter(adapter);
        list.setOnItemClickListener(this);
        PassphraseType currentType = getCurrentTypeFromArguments();
        list.setSelection(adapter.getPositionForType(currentType));

        // Create and return the dialog
        return new AlertDialog.Builder(getActivity())
                .setNegativeButton(R.string.cancel, this)
                .setTitle(R.string.sync_passphrase_type_title)
                .setView(list)
                .create();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == DialogInterface.BUTTON_NEGATIVE) {
            dismiss();
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long typeId) {
        PassphraseType currentType = getCurrentTypeFromArguments();
        // We know this conversion from long to int is safe, because it represents very small
        // enum values.
        PassphraseType type = PassphraseType.fromInternalValue((int) typeId);
        boolean encryptEverythingAllowed = getEncryptEverythingAllowedFromArguments();
        if (currentType.getAllowedTypes(encryptEverythingAllowed).contains(type)) {
            if (typeId != currentType.internalValue()) {
                Listener listener = (Listener) getTargetFragment();
                listener.onPassphraseTypeSelected(type);
            }
            dismiss();
        }
    }

    @VisibleForTesting
    public PassphraseType getCurrentTypeFromArguments() {
        PassphraseType currentType = getArguments().getParcelable(ARG_CURRENT_TYPE);
        if (currentType == null) {
            throw new IllegalStateException("Unable to find argument with current type.");
        }
        return currentType;
    }

    private String getPassphraseDateStringFromArguments() {
        long passphraseTime = getArguments().getLong(ARG_PASSPHRASE_TIME);
        DateFormat df = DateFormat.getDateInstance(DateFormat.MEDIUM);
        return df.format(new Date(passphraseTime));
    }

    private boolean getEncryptEverythingAllowedFromArguments() {
        return getArguments().getBoolean(ARG_ENCRYPT_EVERYTHING_ALLOWED);
    }
}
