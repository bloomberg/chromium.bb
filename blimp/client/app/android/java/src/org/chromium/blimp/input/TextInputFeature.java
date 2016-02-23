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

import org.chromium.base.Log;
import org.chromium.ui.UiUtils;

/**
 * A {@link View} that allows users to enter text into a web page.
 * This is a floating text box which closes when the user hits enter or presses back button.
 */
public class TextInputFeature extends EditText {
    private static final String TAG = "TextInputFeature";

    public TextInputFeature(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOnEditorActionListener(new TextView.OnEditorActionListener() {
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_NEXT
                        || actionId == EditorInfo.IME_ACTION_DONE) {
                    sendTextToBrowser(v.getText().toString());
                    hideIme();
                    return true;
                }
                return false;
            }
        });
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

    private void sendTextToBrowser(String text) {
        Log.d(TAG, "Send to browser : " + text);
        // TODO(shaktisahu): Send data to server.
    }
}
