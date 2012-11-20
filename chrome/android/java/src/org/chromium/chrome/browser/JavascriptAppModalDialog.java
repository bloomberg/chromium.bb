// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.ui.gfx.NativeWindow;

public class JavascriptAppModalDialog {
    private String mTitle;
    private String mMessage;
    private boolean mShouldShowSuppressCheckBox;
    private int mNativeDialogPointer;
    private AlertDialog mDialog;

    private JavascriptAppModalDialog(String title, String message,
            boolean shouldShowSuppressCheckBox) {
        mTitle = title;
        mMessage = message;
        mShouldShowSuppressCheckBox = shouldShowSuppressCheckBox;
    }

    @CalledByNative
    public static JavascriptAppModalDialog createAlertDialog(String title, String message,
            boolean shouldShowSuppressCheckBox) {
        return new JavascriptAppAlertDialog(title, message, shouldShowSuppressCheckBox);
    }

    @CalledByNative
    public static JavascriptAppModalDialog createConfirmDialog(String title, String message,
            boolean shouldShowSuppressCheckBox) {
        return new JavascriptAppConfirmDialog(title, message, shouldShowSuppressCheckBox);
    }

    @CalledByNative
    public static JavascriptAppModalDialog createBeforeUnloadDialog(String title, String message,
            boolean isReload, boolean shouldShowSuppressCheckBox) {
        return new JavascriptAppBeforeUnloadDialog(title, message, isReload,
                shouldShowSuppressCheckBox);
    }

    @CalledByNative
    public static JavascriptAppModalDialog createPromptDialog(String title, String message,
            boolean shouldShowSuppressCheckBox, String defaultPromptText) {
        return new JavascriptAppPromptDialog(title, message, shouldShowSuppressCheckBox,
                defaultPromptText);
    }

    @CalledByNative
    void showJavascriptAppModalDialog(NativeWindow window, int nativeDialogPointer) {
        assert window != null;
        Context context = window.getContext();

        // Cache the native dialog pointer so that we can use it to return the
        // response.
        mNativeDialogPointer = nativeDialogPointer;

        LayoutInflater inflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        ViewGroup dialogLayout = (ViewGroup) inflater.inflate(R.layout.js_modal_dialog, null);

        prepare(dialogLayout);

        mDialog = new AlertDialog.Builder(context)
                .setView(dialogLayout)
                .setOnCancelListener(new DialogInterface.OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        cancel(false);
                    }
                })
                .create();
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.show();
    }

    @CalledByNative
    void dismiss() {
        mDialog.dismiss();
    }

    void prepare(final ViewGroup layout) {
        // Set the title and message.
        TextView titleView = (TextView) layout.findViewById(R.id.js_modal_dialog_title);
        TextView messageView = (TextView) layout.findViewById(R.id.js_modal_dialog_message);
        titleView.setText(mTitle);
        messageView.setText(mMessage);

        // Setup the OK button.
        Button okButton = (Button) layout.findViewById(R.id.js_modal_dialog_button_confirm);
        okButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                boolean suppress = ((CheckBox) layout.findViewById(
                        R.id.suppress_js_modal_dialogs)).isChecked();

                String prompt = ((TextView) layout.findViewById(
                        R.id.js_modal_dialog_prompt)).getText().toString();

                confirm(prompt, suppress);
                mDialog.dismiss();
            }
        });

        // Setup the Cancel button.
        Button cancelButton = (Button) layout.findViewById(R.id.js_modal_dialog_button_cancel);
        cancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                boolean suppress = ((CheckBox) layout.findViewById(
                        R.id.suppress_js_modal_dialogs)).isChecked();
                cancel(suppress);
                mDialog.dismiss();
            }
        });

        // Display the checkbox for supressing dialogs if necessary.
        layout.findViewById(R.id.suppress_js_modal_dialogs).setVisibility(
                mShouldShowSuppressCheckBox ? View.VISIBLE : View.GONE);
    }

    public void confirm(String promptResult, boolean suppressDialogs) {
        nativeDidAcceptAppModalDialog(mNativeDialogPointer, promptResult, suppressDialogs);
    }

    public void cancel(boolean suppressDialogs) {
        nativeDidCancelAppModalDialog(mNativeDialogPointer, suppressDialogs);
    }

    private static class JavascriptAppAlertDialog extends JavascriptAppModalDialog {
        public JavascriptAppAlertDialog(String title, String message,
                boolean shouldShowSuppressCheckBox) {
            super(title, message, shouldShowSuppressCheckBox);
        }

        @Override
        public void prepare(ViewGroup layout) {
            super.prepare(layout);
            layout.findViewById(R.id.js_modal_dialog_button_cancel).setVisibility(View.GONE);
        }
    }

    private static class JavascriptAppConfirmDialog extends JavascriptAppModalDialog {
        public JavascriptAppConfirmDialog(String title, String message,
                boolean shouldShowSuppressCheckBox) {
            super(title, message, shouldShowSuppressCheckBox);
        }

        @Override
        public void prepare(ViewGroup layout) {
            super.prepare(layout);
        }
    }

    private static class JavascriptAppBeforeUnloadDialog extends JavascriptAppConfirmDialog {
        private boolean mIsReload;

        public JavascriptAppBeforeUnloadDialog(String title, String message,
                boolean isReload, boolean shouldShowSuppressCheckBox) {
            super(title, message, shouldShowSuppressCheckBox);
            mIsReload = isReload;
        }

        @Override
        public void prepare(ViewGroup layout) {
            super.prepare(layout);

            // Cancel and confirm button resources are checked in
            // JavascriptAppModalDialog.prepare.
            TextView stayOnThisPage =
                    (TextView) layout.findViewById(R.id.js_modal_dialog_button_cancel);
            stayOnThisPage.setText(mIsReload ?
                    R.string.dont_reload_this_page :
                    R.string.stay_on_this_page);
            TextView leaveThisPage =
                    (TextView) layout.findViewById(R.id.js_modal_dialog_button_confirm);
            leaveThisPage.setText(mIsReload ?
                    R.string.reload_this_page :
                    R.string.leave_this_page);
        }
    }

    private static class JavascriptAppPromptDialog extends JavascriptAppModalDialog {
        private String mDefaultPromptText;

        public JavascriptAppPromptDialog(String title, String message,
                boolean shouldShowSuppressCheckBox, String defaultPromptText) {
            super(title, message, shouldShowSuppressCheckBox);
            mDefaultPromptText = defaultPromptText;
        }

        @Override
        public void prepare(ViewGroup layout) {
            super.prepare(layout);
            EditText prompt = (EditText) layout.findViewById(R.id.js_modal_dialog_prompt);
            prompt.setVisibility(View.VISIBLE);

            if (mDefaultPromptText.length() > 0) {
                prompt.setText(mDefaultPromptText);
                prompt.selectAll();
            }
        }
    }

    private native void nativeDidAcceptAppModalDialog(int nativeJavascriptAppModalDialogAndroid,
            String prompt, boolean suppress);

    private native void nativeDidCancelAppModalDialog(int nativeJavascriptAppModalDialogAndroid,
            boolean suppress);
}
