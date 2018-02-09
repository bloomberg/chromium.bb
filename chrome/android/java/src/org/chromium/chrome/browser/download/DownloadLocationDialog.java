// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.support.v7.app.AlertDialog;
import android.text.InputType;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.download.DownloadDirectoryList;
import org.chromium.chrome.browser.preferences.download.DownloadDirectoryPreference;
import org.chromium.chrome.browser.widget.AlertDialogEditText;

import java.io.File;

/**
 * Dialog that is displayed to ask user where they want to download the file.
 */
public class DownloadLocationDialog
        extends AlertDialog implements DialogInterface.OnClickListener, View.OnFocusChangeListener,
                                       DialogInterface.OnCancelListener {
    private DownloadLocationDialogListener mListener;
    private DownloadDirectoryList mDownloadDirectoryUtil;

    private AlertDialogEditText mFileName;
    private AlertDialogEditText mFileLocation;
    private CheckBox mDontShowAgain;

    /**
     * Interface for a listener to the events related to the DownloadLocationDialog.
     */
    public interface DownloadLocationDialogListener {
        /**
         * Notify listeners that the dialog has been completed.
         * @param returnedPath from the dialog.
         */
        void onComplete(File returnedPath);

        /**
         * Notify listeners that the dialog has been canceled.
         */
        void onCanceled();
    }

    /**
     * Create a DownloadLocationDialog that is displayed before a download begins.
     *
     * @param context of the dialog.
     * @param listener to the updates from the dialog.
     * @param suggestedPath of the download location.
     */
    DownloadLocationDialog(
            Context context, DownloadLocationDialogListener listener, File suggestedPath) {
        super(context, 0);

        mListener = listener;
        mDownloadDirectoryUtil = new DownloadDirectoryList(context);

        setButton(BUTTON_POSITIVE,
                context.getText(R.string.duplicate_download_infobar_download_button), this);
        setButton(BUTTON_NEGATIVE, context.getText(R.string.cancel), this);
        setIcon(0);
        setTitle(context.getString(R.string.download_location_dialog_title));

        LayoutInflater inflater = LayoutInflater.from(context);
        View view = inflater.inflate(R.layout.download_location_dialog, null);

        mFileName = (AlertDialogEditText) view.findViewById(R.id.file_name);
        mFileName.setText(suggestedPath.getName());

        mFileLocation = (AlertDialogEditText) view.findViewById(R.id.file_location);
        // NOTE: This makes the EditText correctly styled but not editable.
        mFileLocation.setInputType(InputType.TYPE_NULL);
        mFileLocation.setOnFocusChangeListener(this);
        setFileLocation(suggestedPath.getParentFile());

        // Automatically check "don't show again" the first time the user is seeing the dialog.
        mDontShowAgain = (CheckBox) view.findViewById(R.id.show_again_checkbox);
        boolean isInitial = PrefServiceBridge.getInstance().getPromptForDownloadAndroid()
                == DownloadPromptStatus.SHOW_INITIAL;
        mDontShowAgain.setChecked(isInitial);

        setView(view);
        setCanceledOnTouchOutside(false);
        setOnCancelListener(this);
    }

    /**
     * Update the string in the file location text view.
     *
     * @param location  The location that the download will go to.
     */
    void setFileLocation(File location) {
        if (mFileLocation == null) return;
        mFileLocation.setText(mDownloadDirectoryUtil.getNameForFile(location));
    }

    // DialogInterface.OnClickListener implementation.

    @Override
    public void onClick(DialogInterface dialogInterface, int which) {
        if (which == BUTTON_POSITIVE) {
            // Get new file path information.
            String fileName = mFileName.getText().toString();
            File fileLocation =
                    mDownloadDirectoryUtil.getFileForName(mFileLocation.getText().toString());
            if (fileLocation != null) {
                mListener.onComplete(new File(fileLocation, fileName));
            } else {
                // If file location returned null, treat as cancellation.
                mListener.onCanceled();
                dismiss();
                return;
            }

            // Update preference to show prompt based on whether checkbox is checked.
            if (mDontShowAgain.isChecked()) {
                PrefServiceBridge.getInstance().setPromptForDownloadAndroid(
                        DownloadPromptStatus.DONT_SHOW);
            } else {
                PrefServiceBridge.getInstance().setPromptForDownloadAndroid(
                        DownloadPromptStatus.SHOW_PREFERENCE);
            }
        } else {
            mListener.onCanceled();
        }

        dismiss();
    }

    // Dialog.OnCancelListener implementation.

    @Override
    public void onCancel(DialogInterface dialogInterface) {
        mListener.onCanceled();
        dismiss();
    }

    // View.OnFocusChange implementation.

    @Override
    public void onFocusChange(View view, boolean hasFocus) {
        // When the file location text view is clicked.
        if (hasFocus) {
            Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                    getContext(), DownloadDirectoryPreference.class.getName());
            getContext().startActivity(intent);
        }
    }
}
