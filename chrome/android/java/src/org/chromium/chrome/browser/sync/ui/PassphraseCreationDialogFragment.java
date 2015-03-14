// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
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
public class PassphraseCreationDialogFragment extends DialogFragment {

    private EditText mEnterPassphrase;
    private EditText mConfirmPassphrase;

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        super.onCreateDialog(savedInstanceState);
        LayoutInflater inflater = getActivity().getLayoutInflater();
        View view = inflater.inflate(R.layout.sync_custom_passphrase, null);
        mEnterPassphrase = (EditText) view.findViewById(R.id.passphrase);
        mConfirmPassphrase = (EditText) view.findViewById(R.id.confirm_passphrase);

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
                String confirmString = mConfirmPassphrase.getText().toString();
                // Only set an error string if either the 'confirm' box has text in it
                // or it has focus.
                if (mConfirmPassphrase.hasFocus() || !confirmString.isEmpty()) {
                    showErrorIfInvalid();
                } else {
                    mConfirmPassphrase.setError(null);
                }
            }
        };
        mEnterPassphrase.addTextChangedListener(validator);
        mConfirmPassphrase.addTextChangedListener(validator);

        // Make sure to display the error text on first entry.
        mConfirmPassphrase.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                showErrorIfInvalid();
            }
        });

        // Make sure to display error text if the user presses a key that
        // doesn't actually change the text. For example, continually presses
        // the backspace button when the field is empty.
        mConfirmPassphrase.setOnKeyListener(new OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (event.getAction() == KeyEvent.ACTION_UP) {
                    showErrorIfInvalid();
                }
                return false;
            }
        });

        mConfirmPassphrase.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    showErrorIfInvalid();
                }
                return false;
            }
        });

        return new AlertDialog.Builder(getActivity(), AlertDialog.THEME_HOLO_LIGHT)
                .setView(view)
                .setTitle(R.string.sync_passphrase_type_custom)
                .setPositiveButton(R.string.ok, null)
                .setNegativeButton(R.string.cancel, null)
                .create();
    }

    @Override
    public void onStart() {
        super.onStart();
        final AlertDialog d = (AlertDialog) getDialog();
        // Override the button's onClick listener. The default gets set in the dialog's onCreate,
        // when it is shown (in super.onStart()), so we have to do this here. Otherwise the dialog
        // will close when the button is clicked regardless of what else we do.
        d.getButton(Dialog.BUTTON_POSITIVE).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (isValidPassphraseText()) {
                    PassphraseDialogFragment.Listener listener =
                            (PassphraseDialogFragment.Listener) getTargetFragment();
                    String passphrase = mConfirmPassphrase.getText().toString();
                    listener.onPassphraseEntered(passphrase, false, true);
                    d.dismiss();
                } else {
                    showErrorIfInvalid();
                }
            }
        });
    }

    private boolean isValidPassphraseText() {
        String str1 = mEnterPassphrase.getText().toString();
        String str2 = mConfirmPassphrase.getText().toString();
        return !str1.isEmpty() && str1.equals(str2);
    }

    private void showErrorIfInvalid() {
        String str1 = mEnterPassphrase.getText().toString();
        String str2 = mConfirmPassphrase.getText().toString();

        String msg = null;
        if (str1.isEmpty()) {
            msg = getString(R.string.sync_passphrase_cannot_be_blank);
        } else if (!str1.equals(str2)) {
            msg = getString(R.string.sync_passphrases_do_not_match);
        }
        mConfirmPassphrase.setError(msg);
    }
}
