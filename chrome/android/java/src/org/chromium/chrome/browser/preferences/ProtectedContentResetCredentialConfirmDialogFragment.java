// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.os.Bundle;

import org.chromium.chrome.R;

/**
 * This is a final confirmation dialog for ressetting device credential on protected content
 * settings page
 */
public class ProtectedContentResetCredentialConfirmDialogFragment extends DialogFragment
        implements DialogInterface.OnClickListener {

    private Listener mListener;

    /**
     * Listener to get notified about user decision on resetting device credential.
     */
    public interface Listener {
        /**
         * Called when user choose to reset device credential.
         */
        public void resetDeviceCredential();
    }

    /**
     * @param listener This is notified when user confirms to reset.
     */
    public ProtectedContentResetCredentialConfirmDialogFragment(Listener listener) {
        mListener = listener;
    }

    /**
     * Create a device reset credential confirmation dialog instance and return it.
     */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        super.onCreateDialog(savedInstanceState);
        return new AlertDialog.Builder(getActivity())
                .setTitle(R.string.protected_content_reset_title)
                .setMessage(R.string.protected_content_reset_message)
                .setNegativeButton(R.string.cancel, this)
                .setPositiveButton(R.string.delete, this)
                .create();
    }

    /**
     * This handles button click events on this dialog.
     */
    @Override
    public void onClick(DialogInterface dialog, int which) {
        switch (which) {
            case AlertDialog.BUTTON_POSITIVE:
                mListener.resetDeviceCredential();
                break;
            case AlertDialog.BUTTON_NEGATIVE:
                break;
            default:
                assert false;
                break;
        }
    }
}
