// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.shell;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * Interaction with the keyboard.
 */
@JNINamespace("mojo::shell")
public class Keyboard {
    @CalledByNative
    private static void showSoftKeyboard(Activity activity) {
        View v = activity.getCurrentFocus();
        InputMethodManager imm =
                (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.showSoftInput(v, InputMethodManager.SHOW_IMPLICIT);
    }

    @CalledByNative
    private static void hideSoftKeyboard(Activity activity) {
        View v = activity.getCurrentFocus();
        InputMethodManager imm =
                (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.hideSoftInputFromWindow(v.getApplicationWindowToken(), 0);
    }
}
