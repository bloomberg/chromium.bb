// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.contents.input;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.core.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.ime.TextInputType;

/**
 * Helper class showing the UI that allows users to enter text into a web page.
 * A pop up is created when user taps on a text area prompting user to start typing.
 * As soon as user submits the text, a spinner is shown which is dismissed only after
 * receiving a hide IME request from the engine.
 */
@JNINamespace("blimp::client")
public class ImeHelperDialog implements WebInputConfirmationPanel.Listener {
    private static final String TAG = "ImeHelperDialog";
    private final Context mContext;

    private String mExistingText;
    private Dialog mDialog;
    private ImeEditText mEditText;

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

    // WebInputConfirmationPanel.Listener implementation.
    @Override
    public void onConfirm() {
        onImeTextEntered(mEditText.getText().toString(), false);
    }

    // WebInputConfirmationPanel.Listener implementation.
    @Override
    public void onCancel() {
        if (mDialog != null) {
            mDialog.dismiss();
        }
    }

    /**
     * Sends the text entered from IME to blimp engine.
     * @param text The text the user entered.
     * @param submit Whether or not to submit the form.
     */
    private void onImeTextEntered(String text, boolean submit) {
        if (mNativeImeHelperDialog == 0) return;

        // Hide the IME.
        InputMethodManager imm =
                (InputMethodManager) mContext.getSystemService(Activity.INPUT_METHOD_SERVICE);
        imm.toggleSoftInput(InputMethodManager.HIDE_IMPLICIT_ONLY, 0);

        mEditText.setTextColor(R.color.disabled_text_color);
        mEditText.setBackgroundResource(R.drawable.dotted_line);

        nativeOnImeTextEntered(mNativeImeHelperDialog, text, submit);
    }

    @CalledByNative
    private void onShowImeRequested(int inputType, String text) {
        if (mDialog != null && mDialog.isShowing()) {
            mDialog.dismiss();
        }

        createTextInputPopup(inputType, text);
    }

    @CalledByNative
    private void onHideImeRequested() {
        if (mDialog == null) return;

        mDialog.dismiss();
    }

    private void createTextInputPopup(int inputType, String existingText) {
        mExistingText = existingText;
        final View dialogView =
                ((Activity) mContext).getLayoutInflater().inflate(R.layout.text_input_popup, null);

        final WebInputConfirmationPanel confirmPanel =
                (WebInputConfirmationPanel) dialogView.findViewById(R.id.submit_panel);
        confirmPanel.setListener(this);

        mDialog = new Dialog(mContext);
        mDialog.setContentView(dialogView);
        mDialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
        mDialog.setCanceledOnTouchOutside(true);

        final TextView tvLabel = (TextView) dialogView.findViewById(R.id.label);
        tvLabel.setText(R.string.blimp_web_input_default_label);

        mEditText = (ImeEditText) dialogView.findViewById(R.id.ime_edit_text);
        mEditText.initialize(mDialog);
        mEditText.setText(existingText);
        setEditorOptions(mEditText, inputType);
        mEditText.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView tv, int actionId, KeyEvent event) {
                switch (actionId) {
                    case EditorInfo.IME_ACTION_NEXT:
                    case EditorInfo.IME_ACTION_DONE:
                    case EditorInfo.IME_ACTION_SEARCH:
                    case EditorInfo.IME_ACTION_GO:
                        confirmPanel.startAnimation();
                        onImeTextEntered(mEditText.getText().toString(), true);
                        return true;
                    default:
                        return false;
                }
            }
        });

        mDialog.show();
        mEditText.requestFocus();
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
                imeOptions |= EditorInfo.IME_ACTION_GO;
                break;
            case TextInputType.PASSWORD:
                inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD;
                imeOptions |= EditorInfo.IME_ACTION_GO;
                break;
            case TextInputType.SEARCH:
                imeOptions |= EditorInfo.IME_ACTION_SEARCH;
                break;
            case TextInputType.EMAIL:
                inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
                imeOptions |= EditorInfo.IME_ACTION_GO;
                break;
            case TextInputType.NUMBER:
                inputType = InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_NORMAL
                        | InputType.TYPE_NUMBER_FLAG_DECIMAL;
                imeOptions |= EditorInfo.IME_ACTION_NEXT;
                break;
            case TextInputType.TELEPHONE:
                inputType = InputType.TYPE_CLASS_PHONE;
                imeOptions |= EditorInfo.IME_ACTION_NEXT;
                break;
            case TextInputType.URL:
                inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI;
                imeOptions |= EditorInfo.IME_ACTION_GO;
                break;
            case TextInputType.TEXT_AREA:
            case TextInputType.CONTENT_EDITABLE:
                inputType |= InputType.TYPE_TEXT_FLAG_MULTI_LINE;
                imeOptions |= EditorInfo.IME_ACTION_NONE;
                break;
            default:
                inputType = InputType.TYPE_CLASS_TEXT;
        }

        editText.setInputType(inputType);
        editText.setImeOptions(imeOptions);
    }

    private native void nativeOnImeTextEntered(
            long nativeImeHelperDialog, String text, boolean submit);
}
