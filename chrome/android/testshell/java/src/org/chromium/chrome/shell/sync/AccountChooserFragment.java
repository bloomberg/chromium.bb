// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell.sync;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.os.Bundle;

import org.chromium.chrome.shell.R;
import org.chromium.sync.signin.AccountManagerHelper;

import java.util.List;

/**
 * The fragment to show when the user is given the option to sign in to Chromium.
 *
 * It lists the available Google accounts on the device and makes the user choose one.
 */
public class AccountChooserFragment extends DialogFragment
        implements DialogInterface.OnClickListener {

    private String[] mAccounts;
    private int mSelectedAccount;

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        List<String> accountsList = AccountManagerHelper.get(getActivity()).getGoogleAccountNames();
        mAccounts = accountsList.toArray(new String[accountsList.size()]);
        return new AlertDialog.Builder(getActivity(), AlertDialog.THEME_HOLO_LIGHT)
                .setTitle(R.string.signin_select_account)
                .setSingleChoiceItems(mAccounts, mSelectedAccount, this)
                .setPositiveButton(R.string.signin_sign_in, this)
                .setNegativeButton(R.string.signin_cancel, this)
                .create();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        switch (which) {
            case DialogInterface.BUTTON_POSITIVE: {
                selectAccount(mAccounts[mSelectedAccount]);
                break;
            }
            case DialogInterface.BUTTON_NEGATIVE: {
                dismiss();
                break;
            }
            default: {
                mSelectedAccount = which;
                break;
            }
        }
    }

    private void selectAccount(String accountName) {
        SyncController.get(getActivity()).signIn(getActivity(), accountName);
    }
}
