// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.contents.input;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.support.v7.app.AlertDialog;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.ime.TextInputType;

/**
 * Helper class showing the UI that allows users to enter text into a web page.
 * A pop up is created when user taps on a text area prompting user to start typing and closes as
 * soon as user hits enter or presses back button.
 */
@JNINamespace("blimp::client")
public class ImeHelperDialog {
    private static final String TAG = "ImeHelperDialog";
    private final Context mContext;

    private AlertDialog mAlertDialog;
    private long mNativeImeHelperDialog;

    @CalledByNative
    private static ImeHelperDialog create(long nativeImeHelperDialog, WindowAndroid windowAndroid) {
        return new ImeHelperDialog(nativeImeHelperDialog,
                WindowAndroid.activityFromContext(windowAndroid.getContext().get()));
    }

    /**
     * Builds a new {@link ImeHelperDialog}.
     * @param nativeImeHelperDialog The pointer to the native ImeHelperDialog.
     * @param context The {@link Context} of the activity.
     */
    private ImeHelperDialog(long nativeImeHelperDialog, Context context) {
        mContext = context;
        mNativeImeHelperDialog = nativeImeHelperDialog;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeImeHelperDialog = 0;
    }

    /**
     * Sends the text entered from IME to blimp engine.
     * @param text The text the user entered.
     */
    private void onImeTextEntered(String text) {
        if (mNativeImeHelperDialog == 0) return;

        nativeOnImeTextEntered(mNativeImeHelperDialog, text);
    }

    @CalledByNative
    private void onShowImeRequested(int inputType, String text) {
        if (mAlertDialog != null && mAlertDialog.isShowing()) {
            mAlertDialog.dismiss();
        }

        createTextInputPopup(inputType, text);
    }

    @CalledByNative
    private void onHideImeRequested() {
        if (mAlertDialog == null) return;

        mAlertDialog.dismiss();
    }

    private void createTextInputPopup(int inputType, String existingText) {
        final View viewPopup =
                ((Activity) mContext).getLayoutInflater().inflate(R.layout.text_input_popup, null);
        final ImeEditText editText = (ImeEditText) viewPopup.findViewById(R.id.ime_edit_text);
        mAlertDialog = new AlertDialog.Builder(mContext)
                               .setTitle(R.string.blimp_ime_dialog_title)
                               .setPositiveButton(R.string.blimp_form_input_ok,
                                       new DialogInterface.OnClickListener() {
                                           @Override
                                           public void onClick(DialogInterface dialog, int which) {
                                               onImeTextEntered(editText.getText().toString());
                                               dialog.dismiss();
                                           }
                                       })
                               .setNegativeButton(R.string.blimp_form_input_cancel,
                                       new DialogInterface.OnClickListener() {
                                           @Override
                                           public void onClick(DialogInterface dialog, int id) {
                                               dialog.dismiss();
                                           }
                                       })
                               .create();
        editText.initialize(mAlertDialog);
        mAlertDialog.setView(viewPopup);
        mAlertDialog.getWindow().setSoftInputMode(
                WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
        mAlertDialog.setCanceledOnTouchOutside(true);

        setEditorOptions(editText, inputType);
        editText.setText(existingText);
        editText.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView tv, int actionId, KeyEvent event) {
                switch (actionId) {
                    case EditorInfo.IME_ACTION_NEXT:
                    case EditorInfo.IME_ACTION_DONE:
                    case EditorInfo.IME_ACTION_SEARCH:
                    case EditorInfo.IME_ACTION_GO:
                        onImeTextEntered(tv.getText().toString());
                        mAlertDialog.dismiss();
                        return true;
                    default:
                        return false;
                }
            }
        });

        editText.requestFocus();

        mAlertDialog.show();
    }

    /**
     * Set the IME options and input type for editor based on the type received from engine.
     * @param editText The edit text for which the IME options are being determined.
     * @param inputTypeEngine Input type received from engine, defined in {@link TextInputType}.
     */
    private static void setEditorOptions(EditText editText, int inputTypeEngine) {
        int inputType = InputType.TYPE_CLASS_TEXT;
        int imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_FLAG_NO_EXTRACT_UI;

        switch (inputTypeEngine) {
            case TextInputType.TEXT:
                inputType = InputType.TYPE_CLASS_TEXT;
                imeOptions = EditorInfo.IME_ACTION_GO;
                break;
            case TextInputType.PASSWORD:
                inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD;
                imeOptions = EditorInfo.IME_ACTION_GO;
                break;
            case TextInputType.SEARCH:
                imeOptions = EditorInfo.IME_ACTION_SEARCH;
                break;
            case TextInputType.EMAIL:
                inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
                imeOptions = EditorInfo.IME_ACTION_GO;
                break;
            case TextInputType.NUMBER:
                inputType = InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_NORMAL
                        | InputType.TYPE_NUMBER_FLAG_DECIMAL;
                imeOptions = EditorInfo.IME_ACTION_NEXT;
                break;
            case TextInputType.TELEPHONE:
                inputType = InputType.TYPE_CLASS_PHONE;
                imeOptions = EditorInfo.IME_ACTION_NEXT;
                break;
            case TextInputType.URL:
                inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI;
                imeOptions = EditorInfo.IME_ACTION_GO;
                break;
            case TextInputType.TEXT_AREA:
            case TextInputType.CONTENT_EDITABLE:
                inputType = InputType.TYPE_TEXT_FLAG_MULTI_LINE;
                imeOptions = EditorInfo.IME_ACTION_NONE;
                break;
            default:
                inputType = InputType.TYPE_CLASS_TEXT;
        }

        editText.setInputType(inputType);
        editText.setImeOptions(imeOptions);
    }

    private native void nativeOnImeTextEntered(long nativeImeHelperDialog, String text);
}
