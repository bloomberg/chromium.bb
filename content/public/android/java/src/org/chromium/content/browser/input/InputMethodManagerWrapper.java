// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.InputMethodManager;

import org.chromium.base.Log;

/**
 * Wrapper around Android's InputMethodManager
 */
public class InputMethodManagerWrapper {
    private static final String TAG = "cr_Ime";

    private final Context mContext;

    public InputMethodManagerWrapper(Context context) {
        Log.d(TAG, "Constructor");
        mContext = context;
    }

    private InputMethodManager getInputMethodManager() {
        return (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
    }

    /**
     * @see android.view.inputmethod.InputMethodManager#restartInput(View)
     */
    public void restartInput(View view) {
        Log.d(TAG, "restartInput");
        getInputMethodManager().restartInput(view);
    }

    /**
     * @see android.view.inputmethod.InputMethodManager#showSoftInput(View, int, ResultReceiver)
     */
    public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
        Log.d(TAG, "showSoftInput");
        getInputMethodManager().showSoftInput(view, flags, resultReceiver);
    }

    /**
     * @see android.view.inputmethod.InputMethodManager#isActive(View)
     */
    public boolean isActive(View view) {
        final boolean active = getInputMethodManager().isActive(view);
        Log.d(TAG, "isActive: " + active);
        return active;
    }

    /**
     * @see InputMethodManager#hideSoftInputFromWindow(IBinder, int, ResultReceiver)
     */
    public boolean hideSoftInputFromWindow(IBinder windowToken, int flags,
            ResultReceiver resultReceiver) {
        Log.d(TAG, "hideSoftInputFromWindow");
        return getInputMethodManager().hideSoftInputFromWindow(windowToken, flags, resultReceiver);
    }

    /**
     * @see android.view.inputmethod.InputMethodManager#updateSelection(View, int, int, int, int)
     */
    public void updateSelection(View view, int selStart, int selEnd,
            int candidatesStart, int candidatesEnd) {
        Log.d(TAG, "updateSelection: SEL [%d, %d], COM [%d, %d]", selStart, selEnd,
                candidatesStart, candidatesEnd);
        getInputMethodManager().updateSelection(view, selStart, selEnd, candidatesStart,
                candidatesEnd);
    }

    /**
     * @see android.view.inputmethod.InputMethodManager#updateCursorAnchorInfo(View,
     * CursorAnchorInfo)
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void updateCursorAnchorInfo(View view, CursorAnchorInfo cursorAnchorInfo) {
        Log.d(TAG, "updateCursorAnchorInfo");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            getInputMethodManager().updateCursorAnchorInfo(view, cursorAnchorInfo);
        }
    }
}
