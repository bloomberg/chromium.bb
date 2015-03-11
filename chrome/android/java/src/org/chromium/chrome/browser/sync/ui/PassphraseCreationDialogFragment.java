// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnFocusChangeListener;
import android.view.View.OnKeyListener;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.chrome.R;

/**
 * Dialog to ask the user to enter a new custom passphrase.
 */
public class PassphraseCreationDialogFragment extends DialogFragment implements
        DialogInterface.OnClickListener {

    private EditText mConfirmPassphrase;

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        super.onCreateDialog(savedInstanceState);
        LayoutInflater inflater = getActivity().getLayoutInflater();
        View v = inflater.inflate(R.layout.sync_custom_passphrase, null);
        final EditText passphrase = (EditText) v.findViewById(R.id.passphrase);
        mConfirmPassphrase = (EditText) v.findViewById(R.id.confirm_passphrase);

        // Check the value of the passphrases when they change
        TextWatcher validator = new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                validatePassphraseText(passphrase, mConfirmPassphrase);
            }
        };
        passphrase.addTextChangedListener(validator);
        mConfirmPassphrase.addTextChangedListener(validator);

        // Make sure to display the error text on first entry.
        mConfirmPassphrase.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                validatePassphraseText(passphrase, mConfirmPassphrase);
            }
        });

        // Make sure to display error text if the user presses a key that
        // doesn't actually change the text. For example, continually presses
        // the backspace button when the field is empty.
        mConfirmPassphrase.setOnKeyListener(new OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (event.getAction() == KeyEvent.ACTION_UP) {
                    validatePassphraseText(passphrase, mConfirmPassphrase);
                }
                return false;
            }
        });

        mConfirmPassphrase.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    validatePassphraseText(passphrase, mConfirmPassphrase);
                    // Disable OK button if passphrases do not match.
                    final AlertDialog dialog = (AlertDialog) getDialog();
                    if (dialog != null) {
                        dialog.getButton(AlertDialog.BUTTON_POSITIVE)
                                .setEnabled(mConfirmPassphrase.getError() == null);
                    }
                }
                return false;
            }
        });

        return new AlertDialog.Builder(getActivity(), AlertDialog.THEME_HOLO_LIGHT)
                .setView(v)
                .setTitle(R.string.sync_passphrase_type_custom)
                .setPositiveButton(R.string.ok, this)
                .setNegativeButton(R.string.cancel, this)
                .create();
    }

    private void validatePassphraseText(EditText passphrase, EditText confirmPassphrase) {
        String str1 = passphrase.getText().toString();
        String str2 = confirmPassphrase.getText().toString();

        // Only set an error string if either the 'confirm' box has text in it
        // or it has focus.
        String msg = null;
        if (confirmPassphrase.hasFocus() || !str2.isEmpty()) {
            if (str2.isEmpty()) {
                msg = getString(R.string.sync_passphrase_cannot_be_blank);
            } else if (!str2.equals(str1)) {
                msg = getString(R.string.sync_passphrases_do_not_match);
            }
        }
        confirmPassphrase.setError(msg);
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == AlertDialog.BUTTON_POSITIVE) {
            if (mConfirmPassphrase.getError() == null) {
                PassphraseDialogFragment.Listener listener =
                        (PassphraseDialogFragment.Listener) getTargetFragment();
                String passphrase = mConfirmPassphrase.getText().toString();
                listener.onPassphraseEntered(passphrase, false, true);
            }
        } else if (which == AlertDialog.BUTTON_NEGATIVE) {
            dismiss();
        }
    }
}
