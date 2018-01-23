// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.content.DialogInterface;
import android.support.v7.app.AlertDialog;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.io.File;

/**
 * Dialog that is displayed to ask user where they want to download the file.
 */
public class DownloadLocationDialog extends AlertDialog implements DialogInterface.OnClickListener {
    private DownloadLocationDialogListener mListener;
    private File mLocationPath;

    /**
     * Interface for a listener to the events related to the DownloadLocationDialog.
     */
    public interface DownloadLocationDialogListener {
        /**
         * Notify listeners that the dialog has been completed.
         * @param returnedPath from the dialog.
         */
        void onComplete(File returnedPath);
    }

    /**
     * Create a DownloadLocationDialog that is displayed before a download begins.
     * @param context of the dialog.
     * @param listener to the updates from the dialog.
     * @param suggestedPath of the download location.
     */
    public DownloadLocationDialog(
            Context context, DownloadLocationDialogListener listener, File suggestedPath) {
        super(context, 0);

        mListener = listener;
        mLocationPath = suggestedPath;

        setButton(BUTTON_POSITIVE, context.getText(R.string.ok), this);
        setButton(BUTTON_NEGATIVE, context.getText(R.string.cancel), this);
        setIcon(0);
        setTitle(context.getString(R.string.download_location_dialog_title));

        LayoutInflater inflater = LayoutInflater.from(context);
        View view = inflater.inflate(R.layout.download_location_dialog, null);
        TextView pathText = (TextView) view.findViewById(R.id.path_text);
        pathText.setText(suggestedPath.getAbsolutePath());
        setView(view);
    }

    @Override
    public void onClick(DialogInterface dialogInterface, int which) {
        // TODO(jming): In future iterations, distinguish which buttons have been clicked.
        mListener.onComplete(mLocationPath);
        dismiss();
    }
}
