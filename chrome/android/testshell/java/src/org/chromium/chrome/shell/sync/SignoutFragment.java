// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell.sync;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.os.Bundle;

import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.shell.R;

/**
 * The fragment to show when the user is given the option to sign out of Chromium.
 */
public class SignoutFragment extends DialogFragment implements DialogInterface.OnClickListener {
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog.Builder(getActivity(), AlertDialog.THEME_HOLO_LIGHT)
                .setTitle(R.string.signout_title)
                .setPositiveButton(R.string.signout_sign_out, this)
                .setNegativeButton(R.string.signout_cancel, this)
                .create();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        switch (which) {
            case DialogInterface.BUTTON_POSITIVE: {
                SigninManager.get(getActivity()).signOut(getActivity(), null);
                break;
            }
            case DialogInterface.BUTTON_NEGATIVE: {
                dismiss();
                break;
            }
            default:
                break;
        }
    }
}
