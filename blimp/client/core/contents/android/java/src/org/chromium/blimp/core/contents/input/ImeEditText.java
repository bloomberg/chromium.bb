// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.contents.input;

import android.app.Dialog;
import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.widget.EditText;

/**
 * A {@link EditText} contained in a dialog which allows users to enter text.
 * The dialog is dismissed on pressing back button.
 */
public class ImeEditText extends EditText {
    // The alert dialog that contains this text box.
    private Dialog mParentDialog;

    public ImeEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the parent view for this view.
     * @param dialog The dialog containing this view.
     */
    public void initialize(Dialog dialog) {
        mParentDialog = dialog;
    }

    /**
     * Dismiss the text input dialog as soon as back button is pressed.
     */
    @Override
    public boolean dispatchKeyEventPreIme(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            mParentDialog.dismiss();
        }
        return super.dispatchKeyEventPreIme(event);
    }
}
