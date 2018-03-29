// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.annotation.SuppressLint;
import android.content.Context;
import android.widget.PopupWindow;

import org.chromium.ui.widget.popups.UiWidgetFactory;

/**
 * The factory that creates VR compatible UI widgets.
 */
public class VrUiWidgetFactory extends UiWidgetFactory {
    public VrUiWidgetFactory() {}

    @Override
    public PopupWindow createPopupWindow(Context context) {
        return null;
    }

    @Override
    public android.widget.Toast createToast(Context context) {
        return new VrToast(context);
    }

    @Override
    @SuppressLint("ShowToast")
    public android.widget.Toast makeToast(Context context, CharSequence text, int duration) {
        android.widget.Toast toast = new VrToast(context);
        // It is tempting to use toast.setText directly instead of creating a tmpToast and
        // calling setView below. However, setText depends on android.widget.Toast.makeText
        // being called first to inflate a subtree which has a Textview. Calling setText without
        // a Textview will result an exception.
        android.widget.Toast tmpToast = android.widget.Toast.makeText(context, text, duration);
        toast.setView(tmpToast.getView());
        toast.setDuration(tmpToast.getDuration());
        return toast;
    }
}
