// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.app.Dialog;
import android.content.Context;
import android.view.View;

import org.chromium.chrome.R;

/** The presenter that shows a {@link ModalDialogView} in an Android dialog. */
public class AppModalPresenter extends ModalDialogManager.Presenter {
    private final Context mContext;
    private Dialog mDialog;

    public AppModalPresenter(Context context) {
        mContext = context;
    }

    @Override
    protected void addDialogView(View dialogView) {
        dialogView.setBackgroundResource(R.drawable.popup_bg);
        mDialog = new Dialog(mContext, R.style.ModalDialogTheme);
        mDialog.setOnCancelListener(dialogInterface
                -> dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE));
        mDialog.setContentView(dialogView);
        mDialog.setCanceledOnTouchOutside(getModalDialog().getCancelOnTouchOutside());
        mDialog.show();
    }

    @Override
    protected void removeDialogView(View dialogView) {
        // Dismiss the currently showing dialog.
        if (mDialog != null) mDialog.dismiss();
        mDialog = null;
    }
}
