// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.CheckBox;
import android.widget.Spinner;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.download.DownloadDirectoryAdapter;
import org.chromium.chrome.browser.widget.AlertDialogEditText;

import java.io.File;

import javax.annotation.Nullable;

/**
 * Dialog that is displayed to ask user where they want to download the file.
 */
public class DownloadLocationDialog extends ModalDialogView {
    private DownloadDirectoryAdapter mDirectoryAdapter;

    private AlertDialogEditText mFileName;
    private Spinner mFileLocation;
    private CheckBox mDontShowAgain;

    /**
     * Create a {@link DownloadLocationDialog} with the given properties.
     *
     * @param controller    Controller that listens to the events from the dialog.
     * @param context       Context from which the dialog emerged.
     * @param suggestedPath The path that was automatically generated, used as a starting point.
     * @return              A {@link DownloadLocationDialog} with the given properties.
     */
    public static DownloadLocationDialog create(
            Controller controller, Context context, File suggestedPath) {
        Params params = new Params();
        params.title = context.getString(R.string.download_location_dialog_title);
        params.positiveButtonTextId = R.string.duplicate_download_infobar_download_button;
        params.negativeButtonTextId = R.string.cancel;
        params.customView =
                LayoutInflater.from(context).inflate(R.layout.download_location_dialog, null);

        return new DownloadLocationDialog(controller, context, suggestedPath, params);
    }

    private DownloadLocationDialog(
            Controller controller, Context context, File suggestedPath, Params params) {
        super(controller, params);

        mDirectoryAdapter = new DownloadDirectoryAdapter(context);

        mFileName = (AlertDialogEditText) params.customView.findViewById(R.id.file_name);
        mFileName.setText(suggestedPath.getName());

        mFileLocation = (Spinner) params.customView.findViewById(R.id.file_location);
        mFileLocation.setAdapter(mDirectoryAdapter);

        mFileLocation.setSelection(mDirectoryAdapter.getSelectedItemId());

        // Automatically check "don't show again" the first time the user is seeing the dialog.
        mDontShowAgain = (CheckBox) params.customView.findViewById(R.id.show_again_checkbox);
        boolean isInitial = PrefServiceBridge.getInstance().getPromptForDownloadAndroid()
                == DownloadPromptStatus.SHOW_INITIAL;
        mDontShowAgain.setChecked(isInitial);
    }

    // Helper methods available to DownloadLocationDialogBridge.

    /**
     * @return  The text that the user inputted as the name of the file.
     */
    @Nullable
    String getFileName() {
        if (mFileName == null) return null;
        return mFileName.getText().toString();
    }

    /**
     * @return  The file path based on what the user selected as the location of the file.
     */
    @Nullable
    File getFileLocation() {
        if (mFileLocation == null) return null;
        DownloadDirectoryAdapter.DirectoryOption selected =
                (DownloadDirectoryAdapter.DirectoryOption) mFileLocation.getSelectedItem();
        return selected.getLocation();
    }

    /**
     * @return  Whether the "don't show again" checkbox is checked.
     */
    boolean getDontShowAgain() {
        return mDontShowAgain != null && mDontShowAgain.isChecked();
    }
}
