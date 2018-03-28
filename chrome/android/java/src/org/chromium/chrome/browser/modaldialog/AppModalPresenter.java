// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.app.Activity;
import android.app.Dialog;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.AlwaysDismissedDialog;

/** The presenter that shows a {@link ModalDialogView} in an Android dialog. */
public class AppModalPresenter extends ModalDialogManager.Presenter {
    private final Activity mActivity;
    private Dialog mDialog;

    public AppModalPresenter(Activity activity) {
        mActivity = activity;
    }

    @Override
    protected void addDialogView(View dialogView) {
        mDialog = new AlwaysDismissedDialog(mActivity, R.style.ModalDialogTheme);
        mDialog.setOnCancelListener(dialogInterface -> cancelCurrentDialog());
        ViewGroup container = (ViewGroup) LayoutInflater.from(mActivity).inflate(
                R.layout.modal_dialog_container, null);
        mDialog.setContentView(container);
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(ViewGroup.MarginLayoutParams.WRAP_CONTENT,
                        ViewGroup.MarginLayoutParams.WRAP_CONTENT, Gravity.CENTER);
        container.addView(dialogView, params);
        if (getModalDialog().getCancelOnTouchOutside()) {
            mDialog.setCanceledOnTouchOutside(true);
            // The dialog container covers the entire screen. To achieve the cancel on touch outside
            // behavior cancel the dialog if the container is touched.
            container.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    mDialog.cancel();
                }
            });
        }
        mDialog.show();
    }

    @Override
    protected void removeDialogView(View dialogView) {
        // Dismiss the currently showing dialog.
        if (mDialog != null) mDialog.dismiss();
        mDialog = null;
    }
}
