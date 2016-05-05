// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.ui.base.WindowAndroid;

/**
 * The auto sign-in first run experience dialog is shown instead of usual auto sign-in snackbar
 * when the user first encounters the auto sign-in feature.
 */
public class AutoSigninFirstRunDialog
        extends DialogFragment implements DialogInterface.OnClickListener {
    private final Context mContext;
    private final String mTitle;
    private final String mExplanation;
    private final int mExplanationLinkStart;
    private final int mExplanationLinkEnd;
    private final String mOkButtonText;
    private final String mTurnOffButtonText;
    private long mNativeAutoSigninFirstRunDialog;
    private AlertDialog mDialog;

    private AutoSigninFirstRunDialog(Context context, long nativeAutoSigninFirstRunDialog,
            String title, String explanation, int explanationLinkStart, int explanationLinkEnd,
            String okButtonText, String turnOffButtonText) {
        mNativeAutoSigninFirstRunDialog = nativeAutoSigninFirstRunDialog;
        mContext = context;
        mTitle = title;
        mExplanation = explanation;
        mExplanationLinkStart = explanationLinkStart;
        mExplanationLinkEnd = explanationLinkEnd;
        mOkButtonText = okButtonText;
        mTurnOffButtonText = turnOffButtonText;
    }

    @CalledByNative
    private static AutoSigninFirstRunDialog createDialog(WindowAndroid windowAndroid,
            long nativeAutoSigninFirstRunDialog, String title, String explanation,
            int explanationLinkStart, int explanationLinkEnd, String okButtonText,
            String turnOffButtonText) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return null;

        AutoSigninFirstRunDialog dialog = new AutoSigninFirstRunDialog(activity,
                nativeAutoSigninFirstRunDialog, title, explanation, explanationLinkStart,
                explanationLinkEnd, okButtonText, turnOffButtonText);
        dialog.show(activity.getFragmentManager(), null);
        return dialog;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        final AlertDialog.Builder builder =
                new AlertDialog.Builder(mContext, R.style.AlertDialogTheme)
                        .setTitle(mTitle)
                        .setPositiveButton(mOkButtonText, this)
                        .setNegativeButton(mTurnOffButtonText, this);
        View view = LayoutInflater.from(mContext).inflate(
                R.layout.auto_sign_in_first_run_dialog, null);
        TextView summaryView = (TextView) view.findViewById(R.id.summary);

        if (mExplanationLinkStart != mExplanationLinkEnd && mExplanationLinkEnd != 0) {
            SpannableString spanableExplanation = new SpannableString(mExplanation);
            spanableExplanation.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View view) {
                    nativeOnLinkClicked(mNativeAutoSigninFirstRunDialog);
                    mDialog.dismiss();
                }
            }, mExplanationLinkStart, mExplanationLinkEnd, Spanned.SPAN_INCLUSIVE_INCLUSIVE);
            summaryView.setText(spanableExplanation);
            summaryView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            summaryView.setText(mExplanation);
            summaryView.setMovementMethod(LinkMovementMethod.getInstance());
        }
        builder.setView(view);

        mDialog = builder.create();
        mDialog.setCanceledOnTouchOutside(false);
        return mDialog;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) dismiss();
    }

    @Override
    public void onClick(DialogInterface dialog, int whichButton) {
        if (whichButton == DialogInterface.BUTTON_NEGATIVE) {
            nativeOnTurnOffClicked(mNativeAutoSigninFirstRunDialog);
        } else if (whichButton == DialogInterface.BUTTON_POSITIVE) {
            nativeOnOkClicked(mNativeAutoSigninFirstRunDialog);
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        destroy();
        mDialog = null;
    }

    private void destroy() {
        assert mNativeAutoSigninFirstRunDialog != 0;
        nativeDestroy(mNativeAutoSigninFirstRunDialog);
        mNativeAutoSigninFirstRunDialog = 0;
    }

    private native void nativeOnTurnOffClicked(long nativeAutoSigninFirstRunDialogAndroid);
    private native void nativeOnOkClicked(long nativeAutoSigninFirstRunDialogAndroid);
    private native void nativeDestroy(long nativeAutoSigninFirstRunDialogAndroid);
    private native void nativeOnLinkClicked(long nativeAutoSigninFirstRunDialogAndroid);
}
