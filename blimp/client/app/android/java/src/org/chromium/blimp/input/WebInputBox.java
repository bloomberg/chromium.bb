// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.input;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.session.BlimpClientSession;
import org.chromium.ui.UiUtils;

/**
 * A {@link View} that allows users to enter text into a web page.
 * This is a floating text box which closes when the user hits enter or presses back button.
 */
@JNINamespace("blimp::client")
public class WebInputBox extends EditText {
    private static final String TAG = "WebInputBox";
    private long mNativeWebInputBoxPtr;

    public WebInputBox(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOnEditorActionListener(new TextView.OnEditorActionListener() {
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_NEXT
                        || actionId == EditorInfo.IME_ACTION_DONE) {
                    onImeTextEntered(v.getText().toString());
                    hideIme();
                    return true;
                }
                return false;
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
            setVisibility(View.GONE);
        }
        return super.dispatchKeyEventPreIme(event);
    }

    /**
     *  Brings up the IME along with the edit text above it.
     */
    public void showIme() {
        setVisibility(View.VISIBLE);
        requestFocus();
        UiUtils.showKeyboard(this);
    }

    /**
     * Hides the edit text along with the IME.
     */
    private void hideIme() {
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
    private void onImeRequested(boolean show) {
        if (show) {
            showIme();
        } else {
            hideIme();
        }
    }

    private native long nativeInit(BlimpClientSession blimpClientSession);
    private native void nativeDestroy(long nativeWebInputBox);
    private native void nativeOnImeTextEntered(long nativeWebInputBox, String text);
}
