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
import org.chromium.content.app.AppResource;
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

        assert AppResource.LAYOUT_JS_MODAL_DIALOG != 0;
        ViewGroup dialogLayout = (ViewGroup) inflater.inflate(AppResource.LAYOUT_JS_MODAL_DIALOG,
                null);

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
        assert AppResource.ID_JS_MODAL_DIALOG_TEXT_TITLE != 0;
        assert AppResource.ID_JS_MODAL_DIALOG_TEXT_MESSAGE != 0;

        TextView titleView = (TextView) layout.findViewById(
                AppResource.ID_JS_MODAL_DIALOG_TEXT_TITLE);
        TextView messageView = (TextView) layout.findViewById(
                AppResource.ID_JS_MODAL_DIALOG_TEXT_MESSAGE);
        titleView.setText(mTitle);
        messageView.setText(mMessage);

        // Setup the OK button.
        assert AppResource.ID_JS_MODAL_DIALOG_BUTTON_CONFIRM != 0;
        assert AppResource.ID_JS_MODAL_DIALOG_CHECKBOX_SUPPRESS_DIALOGS != 0;

        Button okButton = (Button) layout.findViewById(
                AppResource.ID_JS_MODAL_DIALOG_BUTTON_CONFIRM);
        okButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                boolean suppress = ((CheckBox) layout.findViewById(
                        AppResource.ID_JS_MODAL_DIALOG_CHECKBOX_SUPPRESS_DIALOGS)).isChecked();

                assert AppResource.ID_JS_MODAL_DIALOG_TEXT_PROMPT != 0;
                String prompt = ((TextView) layout.findViewById(
                        AppResource.ID_JS_MODAL_DIALOG_TEXT_PROMPT)).getText().toString();

                confirm(prompt, suppress);
                mDialog.dismiss();
            }
        });

        // Setup the Cancel button.
        assert AppResource.ID_JS_MODAL_DIALOG_BUTTON_CANCEL != 0;

        Button cancelButton = (Button) layout.findViewById(
                AppResource.ID_JS_MODAL_DIALOG_BUTTON_CANCEL);
        cancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                boolean suppress = ((CheckBox) layout.findViewById(
                        AppResource.ID_JS_MODAL_DIALOG_CHECKBOX_SUPPRESS_DIALOGS)).isChecked();
                cancel(suppress);
                mDialog.dismiss();
            }
        });

        // Display the checkbox for supressing dialogs if necessary.
        layout.findViewById(AppResource.ID_JS_MODAL_DIALOG_CHECKBOX_SUPPRESS_DIALOGS).setVisibility(
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
            layout.findViewById(AppResource.ID_JS_MODAL_DIALOG_BUTTON_CANCEL)
                    .setVisibility(View.GONE);
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
            assert AppResource.STRING_JS_MODAL_DIALOG_DONT_RELOAD_THIS_PAGE != 0;
            assert AppResource.STRING_JS_MODAL_DIALOG_LEAVE_THIS_PAGE != 0;
            assert AppResource.STRING_JS_MODAL_DIALOG_RELOAD_THIS_PAGE != 0;
            assert AppResource.STRING_JS_MODAL_DIALOG_STAY_ON_THIS_PAGE != 0;

            TextView stayOnThisPage =
                    (TextView) layout.findViewById(AppResource.ID_JS_MODAL_DIALOG_BUTTON_CANCEL);
            stayOnThisPage.setText(mIsReload ?
                    AppResource.STRING_JS_MODAL_DIALOG_DONT_RELOAD_THIS_PAGE :
                    AppResource.STRING_JS_MODAL_DIALOG_STAY_ON_THIS_PAGE);
            TextView leaveThisPage =
                    (TextView) layout.findViewById(AppResource.ID_JS_MODAL_DIALOG_BUTTON_CONFIRM);
            leaveThisPage.setText(mIsReload ?
                    AppResource.STRING_JS_MODAL_DIALOG_RELOAD_THIS_PAGE :
                    AppResource.STRING_JS_MODAL_DIALOG_LEAVE_THIS_PAGE);
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
            EditText prompt = (EditText) layout.findViewById(
                    AppResource.ID_JS_MODAL_DIALOG_TEXT_PROMPT);
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
