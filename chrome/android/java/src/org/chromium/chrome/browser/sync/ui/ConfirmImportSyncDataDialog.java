// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.app.FragmentManager;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BrowsingDataType;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PrefServiceBridge.OnClearBrowsingDataListener;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.widget.RadioButtonWithDescription;

import java.util.Arrays;
import java.util.List;

/**
 * A dialog that is displayed when the user switches the account they are syncing to. It gives the
 * option to merge the data of the two accounts or to keep them separate.
 */
public class ConfirmImportSyncDataDialog extends DialogFragment
        implements DialogInterface.OnClickListener, OnClearBrowsingDataListener {

    /**
     * Callback for completion of the dialog.
     */
    public interface Listener {
        /**
         * The user has completed the dialog using the positive button. If requested, the previous
         * sync data has been cleared.
         */
        public void onConfirm();
    }

    /**
     * The situation ConfirmImportSyncDataDialog is created for - whether the user had previously
     * been signed into another account, had signed out then signed into a different one, or
     * if they directly switched accounts. This changes the strings displayed.
     */
    public enum ImportSyncType { SWITCHING_SYNC_ACCOUNTS, PREVIOUS_DATA_FOUND }

    private static final String CONFIRM_IMPORT_SYNC_DATA_DIALOG_TAG =
            "sync_account_switch_import_data_tag";

    private static final int[] SYNC_DATA_TYPES = {
            BrowsingDataType.HISTORY,
            BrowsingDataType.CACHE,
            BrowsingDataType.COOKIES,
            BrowsingDataType.PASSWORDS,
            BrowsingDataType.FORM_DATA
    };

    private static final String KEY_OLD_ACCOUNT_NAME = "lastAccountName";
    private static final String KEY_NEW_ACCOUNT_NAME = "newAccountName";
    private static final String KEY_IMPORT_SYNC_TYPE = "importSyncType";

    private RadioButtonWithDescription mConfirmImportOption;
    private RadioButtonWithDescription mKeepSeparateOption;

    private Listener mListener;

    private static ConfirmImportSyncDataDialog newInstance(
            String oldAccountName, String newAccountName, ImportSyncType importSyncType) {

        ConfirmImportSyncDataDialog fragment = new ConfirmImportSyncDataDialog();
        Bundle args = new Bundle();
        args.putString(KEY_OLD_ACCOUNT_NAME, oldAccountName);
        args.putString(KEY_NEW_ACCOUNT_NAME, newAccountName);
        args.putSerializable(KEY_IMPORT_SYNC_TYPE, importSyncType);
        fragment.setArguments(args);
        return fragment;
    }

    /**
     * Creates and shows a new instance of ConfirmImportSyncDataFragment, a dialog that gives the
     * user the option to merge data between the account they are attempting to sign in to and the
     * account they were previously signed into, or to keep the data separate.
     * @param oldAccountName  The previous sync account name.
     * @param newAccountName  The potential next sync account name.
     * @param importSyncType  The situation the dialog is created in - either when directly changing
     *                        the sync account or signing in after being signed out (this changes
     *                        displayed strings).
     * @param fragmentManager FragmentManager to attach the dialog to.
     * @param callback        Callback to be called if the user completes the dialog (as opposed to
     *                        hitting cancel).
     */
    public static void showNewInstance(String oldAccountName, String newAccountName,
            ImportSyncType importSyncType, FragmentManager fragmentManager, Listener callback) {
        ConfirmImportSyncDataDialog confirmSync =
                newInstance(oldAccountName, newAccountName, importSyncType);

        confirmSync.setListener(callback);
        confirmSync.show(fragmentManager, CONFIRM_IMPORT_SYNC_DATA_DIALOG_TAG);
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        String oldAccountName = getArguments().getString(KEY_OLD_ACCOUNT_NAME);
        String newAccountName = getArguments().getString(KEY_NEW_ACCOUNT_NAME);
        ImportSyncType importSyncType =
                (ImportSyncType) getArguments().getSerializable(KEY_IMPORT_SYNC_TYPE);

        LayoutInflater inflater = getActivity().getLayoutInflater();
        View v = inflater.inflate(R.layout.confirm_import_sync_data, null);

        TextView prompt = (TextView) v.findViewById(R.id.sync_import_data_prompt);

        if (importSyncType == ImportSyncType.SWITCHING_SYNC_ACCOUNTS) {
            prompt.setText(getActivity().getString(
                    R.string.sync_import_data_prompt_switching_accounts,
                    newAccountName, oldAccountName));
        } else {
            prompt.setText(getActivity().getString(
                    R.string.sync_import_data_prompt_existing_data,
                    newAccountName, oldAccountName));
        }

        mConfirmImportOption = (RadioButtonWithDescription)
                v.findViewById(R.id.sync_confirm_import_choice);
        mKeepSeparateOption = (RadioButtonWithDescription)
                v.findViewById(R.id.sync_keep_separate_choice);

        mConfirmImportOption.setDescriptionText(getActivity().getString(
                R.string.sync_import_existing_data_subtext, newAccountName));
        mKeepSeparateOption.setDescriptionText(getActivity().getString(
                R.string.sync_keep_existing_data_separate_subtext, newAccountName, oldAccountName));

        List<RadioButtonWithDescription> radioGroup =
                Arrays.asList(mConfirmImportOption, mKeepSeparateOption);
        mConfirmImportOption.setRadioButtonGroup(radioGroup);
        mKeepSeparateOption.setRadioButtonGroup(radioGroup);

        if (importSyncType == ImportSyncType.SWITCHING_SYNC_ACCOUNTS) {
            mKeepSeparateOption.setChecked(true);
        } else {
            mConfirmImportOption.setChecked(true);
        }

        return new AlertDialog.Builder(getActivity(), R.style.AlertDialogTheme)
                .setTitle(R.string.sync_import_data_title)
                .setPositiveButton(R.string.continue_button, this)
                .setNegativeButton(R.string.cancel, this)
                .setView(v)
                .create();
    }

    private void setListener(Listener listener) {
        assert mListener == null;
        mListener = listener;
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which != AlertDialog.BUTTON_POSITIVE) {
            RecordUserAction.record("Signin_ImportDataPrompt_Cancel");
            return;
        }
        if (mListener == null) return;

        assert mConfirmImportOption.isChecked() ^ mKeepSeparateOption.isChecked();

        if (mConfirmImportOption.isChecked()) {
            RecordUserAction.record("Signin_ImportDataPrompt_ImportData");
            mListener.onConfirm();
        } else {
            RecordUserAction.record("Signin_ImportDataPrompt_DontImport");

            final BookmarkModel model = new BookmarkModel();
            model.runAfterBookmarkModelLoaded(new Runnable() {
                @Override
                public void run() {
                    model.removeAllUserBookmarks();
                    model.destroy();
                    PrefServiceBridge.getInstance().clearBrowsingData(
                            ConfirmImportSyncDataDialog.this, SYNC_DATA_TYPES);
                }
            });
        }
    }

    @Override
    public void onBrowsingDataCleared() {
        SigninManager.get(getActivity()).clearLastSignedInUser();
        if (mListener != null) mListener.onConfirm();
    }
}

