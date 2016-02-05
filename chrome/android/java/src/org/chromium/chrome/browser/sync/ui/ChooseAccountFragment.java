// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.Activity;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.SigninManager;

import java.util.List;

/**
 * A dialog that lets the user choose which Google account to use when signing in to Chrome.
 */
public class ChooseAccountFragment extends DialogFragment implements OnClickListener {

    protected String[] mAccounts;

    protected int mSelectedAccount;

    // TODO(skym): Fragments should have empty constructors, crbug.com/580093.
    public ChooseAccountFragment(List<String> accountNames) {
        mAccounts = accountNames.toArray(new String[accountNames.size()]);
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog.Builder(getActivity(), R.style.AlertDialogTheme)
                .setSingleChoiceItems(mAccounts, mSelectedAccount, this)
                .setPositiveButton(R.string.choose_account_sign_in, this)
                .setNegativeButton(R.string.cancel, this)
                .setTitle(R.string.sign_in_google_account)
                .create();
    }

    private static void signIn(final Activity activity, String accountName) {
        RecordUserAction.record("Signin_Signin_FromSettings");
        SigninManager.get(activity).signIn(accountName, activity, null);
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        switch (which) {
            case AlertDialog.BUTTON_POSITIVE: {
                signIn(getActivity(), mAccounts[mSelectedAccount]);
                break;
            }
            default: {
                mSelectedAccount = which;
                break;
            }
        }
    }
}
