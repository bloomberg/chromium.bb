// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.view.View;

import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;

/**
 * This class implements a VrAlertDialog which is similar to Android AlertDialog in VR.
 */
public class VrAlertDialog extends AlertDialog {
    private VrDialogManager mVrDialogManager;
    private ModalDialogManager mModalDialogManager;
    private ModalDialogView mModalDialogView;
    private boolean mIsShowing;
    private CharSequence mMessage;
    private DialogButton mButtonPositive;
    private DialogButton mButtonNegative;
    protected View mView;

    public VrAlertDialog(Context context, VrDialogManager vrDialogManager,
            ModalDialogManager modalDialogManager) {
        super(context);
        mVrDialogManager = vrDialogManager;
        mModalDialogManager = modalDialogManager;
    }

    private class DialogButton {
        private int mId;
        private String mText;
        private DialogInterface.OnClickListener mListener;
        DialogButton(int id, String text, DialogInterface.OnClickListener listener) {
            mId = id;
            mText = text;
            mListener = listener;
        }

        public int getId() {
            return mId;
        }
        public String getText() {
            return mText;
        }
        public DialogInterface.OnClickListener getListener() {
            return mListener;
        }
    }

    /**
     * Builds a ModalDialogView and ask ModalDialogManager to show it.
     */
    @Override
    public void show() {
        mModalDialogView = createView();
        mModalDialogManager.showDialog(mModalDialogView, ModalDialogManager.APP_MODAL);
        mIsShowing = true;
    }

    /**
     * Set the main view
     */
    @Override
    public void setView(View view) {
        mView = view;
    }

    /**
     * Set the message of the AlertDialog.
     * Note that message and view cannot be used at the same time.
     * If a view is set, then the message will be ignored.
     */
    @Override
    public void setMessage(CharSequence message) {
        mMessage = message;
    }

    /**
     * Add button to the list of buttons of this Dialog.
     */
    @Override
    public void setButton(
            int whichButton, CharSequence text, DialogInterface.OnClickListener listener) {
        assert(whichButton == DialogInterface.BUTTON_POSITIVE
                || whichButton == DialogInterface.BUTTON_NEGATIVE);
        if (whichButton == DialogInterface.BUTTON_POSITIVE) {
            mButtonPositive =
                    new DialogButton(ModalDialogView.BUTTON_POSITIVE, text.toString(), listener);
        } else if (whichButton == DialogInterface.BUTTON_NEGATIVE) {
            mButtonNegative =
                    new DialogButton(ModalDialogView.BUTTON_NEGATIVE, text.toString(), listener);
        }
    }

    /**
     * Dismiss the dialog.
     */
    @Override
    public void dismiss() {
        mIsShowing = false;
        mModalDialogManager.cancelDialog(mModalDialogView);
    }

    private ModalDialogView createView() {
        ModalDialogView.Controller controller = new ModalDialogView.Controller() {
            @Override
            public void onCancel() {}

            @Override
            public void onClick(int buttonType) {
                if (buttonType == ModalDialogView.BUTTON_POSITIVE) {
                    mButtonPositive.getListener().onClick(null, mButtonPositive.getId());
                } else if (buttonType == ModalDialogView.BUTTON_NEGATIVE) {
                    mButtonNegative.getListener().onClick(null, mButtonNegative.getId());
                }
                dismiss();
            }

            @Override
            public void onDismiss() {}
        };
        final ModalDialogView.Params params = new ModalDialogView.Params();

        assert(mView == null || mMessage == null);
        if (mView != null) {
            params.customView = mView;
        } else if (mMessage != null) {
            params.message = mMessage.toString();
        }

        if (mButtonPositive != null) params.positiveButtonText = mButtonPositive.getText();
        if (mButtonNegative != null) params.negativeButtonText = mButtonNegative.getText();

        return new ModalDialogView(controller, params);
    }
}
