// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.jsdialog;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.ui.base.WindowAndroid;

/**
 * The controller to communicate with native JavaScriptDialogAndroid for a tab modal JavaScript
 * dialog. This can be an alert dialog, a prompt dialog or a confirm dialog.
 */
public class JavascriptTabModalDialog extends JavascriptModalDialog {
    private long mNativeDialogPointer;

    /**
     * Constructor for initializing contents to be shown on the dialog.
     */
    private JavascriptTabModalDialog(
            String title, String message, String promptText, int negativeButtonTextId) {
        super(title, message, promptText, false, R.string.ok, negativeButtonTextId);
    }

    @CalledByNative
    private static JavascriptTabModalDialog createAlertDialog(String title, String message) {
        return new JavascriptTabModalDialog(title, message, null, 0);
    }

    @CalledByNative
    private static JavascriptTabModalDialog createConfirmDialog(String title, String message) {
        return new JavascriptTabModalDialog(title, message, null, R.string.cancel);
    }

    @CalledByNative
    private static JavascriptTabModalDialog createPromptDialog(
            String title, String message, String defaultPromptText) {
        return new JavascriptTabModalDialog(title, message, defaultPromptText, R.string.cancel);
    }

    @CalledByNative
    private void showDialog(WindowAndroid window, long nativeDialogPointer) {
        assert window != null;
        ChromeActivity activity = (ChromeActivity) window.getActivity().get();
        // If the activity has gone away, then just clean up the native pointer.
        if (activity == null) {
            nativeCancel(nativeDialogPointer, false);
            return;
        }

        // Cache the native dialog pointer so that we can use it to return the response.
        mNativeDialogPointer = nativeDialogPointer;
        show(activity, ModalDialogManager.ModalDialogType.TAB);
    }

    @CalledByNative
    private String getUserInput() {
        return mDialogCustomView.getPromptText();
    }

    @CalledByNative
    private void dismiss() {
        dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        mNativeDialogPointer = 0;
    }

    /**
     * Sends notification to native that the user accepts the dialog.
     * @param promptResult The text edited by user.
     */
    @Override
    protected void accept(String promptResult, boolean suppressDialogs) {
        if (mNativeDialogPointer == 0) return;
        nativeAccept(mNativeDialogPointer, promptResult);
    }

    /**
     * Sends notification to native that the user cancels the dialog.
     */
    @Override
    protected void cancel(boolean buttonClicked, boolean suppressDialogs) {
        if (mNativeDialogPointer == 0) return;
        nativeCancel(mNativeDialogPointer, buttonClicked);
    }

    private native void nativeAccept(long nativeJavaScriptDialogAndroid, String prompt);
    private native void nativeCancel(long nativeJavaScriptDialogAndroid, boolean buttonClicked);
}
