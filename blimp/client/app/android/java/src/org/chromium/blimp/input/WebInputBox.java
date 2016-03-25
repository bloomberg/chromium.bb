// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.input;

import android.app.Activity;
import android.content.Context;
import android.text.InputType;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.session.BlimpClientSession;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ime.TextInputType;

/**
 * A {@link View} that allows users to enter text into a web page.
 * This is a floating text box which closes when the user hits enter or presses back button.
 */
@JNINamespace("blimp::client")
public class WebInputBox extends EditText {
    private static final String TAG = "WebInputBox";
    private long mNativeWebInputBoxPtr;
    private Context mContext;

    public WebInputBox(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        setOnEditorActionListener(new TextView.OnEditorActionListener() {
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                switch (actionId) {
                    case EditorInfo.IME_ACTION_NEXT:
                    case EditorInfo.IME_ACTION_DONE:
                    case EditorInfo.IME_ACTION_SEARCH:
                    case EditorInfo.IME_ACTION_GO:
                        onImeTextEntered(v.getText().toString());
                        hideIme();
                        return true;
                    default:
                        return false;
                }
            }
        });
    }

    /**
     * To be called when the native library is loaded so that this class can initialize its native
     * components.
     * @param blimpClientSession The {@link BlimpClientSession} that contains the features
     *                           required by the native components of the WebInputBox.
     */
    public void initialize(BlimpClientSession blimpClientSession) {
        assert mNativeWebInputBoxPtr == 0;

        mNativeWebInputBoxPtr = nativeInit(blimpClientSession);
    }

    /**
     * To be called when this class should be torn down.  This {@link View} should not be used after
     * this.
     */
    public void destroy() {
        if (mNativeWebInputBoxPtr == 0) return;

        nativeDestroy(mNativeWebInputBoxPtr);
        mNativeWebInputBoxPtr = 0;
    }

    @Override
    public boolean dispatchKeyEventPreIme(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            ((Activity) mContext)
                    .getWindow()
                    .setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
            setVisibility(View.GONE);
        }
        return super.dispatchKeyEventPreIme(event);
    }

    /**
     *  Brings up the IME along with the edit text above it.
     */
    private void showIme() {
        // TODO(shaktisahu): Find a better way to prevent resize (crbug/596653).
        ((Activity) mContext)
                .getWindow()
                .setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        setVisibility(View.VISIBLE);
        requestFocus();
        UiUtils.showKeyboard(this);
    }

    /**
     * Hides the edit text along with the IME.
     */
    private void hideIme() {
        ((Activity) mContext)
                .getWindow()
                .setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
        setText("");
        setVisibility(View.GONE);
        UiUtils.hideKeyboard(this);
    }

    /**
     * Sends the text entered from IME to blimp engine.
     * @param text The text the user entered.
     */
    private void onImeTextEntered(String text) {
        if (mNativeWebInputBoxPtr == 0) return;

        nativeOnImeTextEntered(mNativeWebInputBoxPtr, text);
    }

    @CalledByNative
    private void onShowImeRequested(int inputType, String text) {
        setEditorOptions(inputType);
        setText(text);
        // Set the cursor at the end.
        setSelection(getText().length());
        showIme();
    }

    @CalledByNative
    private void onHideImeRequested() {
        hideIme();
    }

    /**
     * Set the IME options and input type based on the input type received from engine.
     * @param inputType text input type.
     */
    private void setEditorOptions(int inputType) {
        setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        switch (inputType) {
            case TextInputType.TEXT:
                setInputType(InputType.TYPE_CLASS_TEXT);
                setImeOptions(EditorInfo.IME_ACTION_GO);
                break;
            case TextInputType.PASSWORD:
                setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
                setImeOptions(EditorInfo.IME_ACTION_GO);
                break;
            case TextInputType.SEARCH:
                setImeOptions(EditorInfo.IME_ACTION_SEARCH);
                break;
            case TextInputType.EMAIL:
                setInputType(
                        InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
                setImeOptions(EditorInfo.IME_ACTION_GO);
                break;
            case TextInputType.NUMBER:
                setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_NORMAL
                        | InputType.TYPE_NUMBER_FLAG_DECIMAL);
                setImeOptions(EditorInfo.IME_ACTION_NEXT);
                break;
            case TextInputType.TELEPHONE:
                setInputType(InputType.TYPE_CLASS_PHONE);
                setImeOptions(EditorInfo.IME_ACTION_NEXT);
                break;
            case TextInputType.URL:
                setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
                setImeOptions(EditorInfo.IME_ACTION_GO);
                break;
            case TextInputType.TEXT_AREA:
            case TextInputType.CONTENT_EDITABLE:
                setInputType(InputType.TYPE_TEXT_FLAG_MULTI_LINE);
                setImeOptions(EditorInfo.IME_ACTION_NONE);
                break;
            default:
                setInputType(InputType.TYPE_CLASS_TEXT);
        }
    }

    private native long nativeInit(BlimpClientSession blimpClientSession);
    private native void nativeDestroy(long nativeWebInputBox);
    private native void nativeOnImeTextEntered(long nativeWebInputBox, String text);
}
