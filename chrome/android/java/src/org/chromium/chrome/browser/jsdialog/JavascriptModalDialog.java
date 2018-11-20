// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.jsdialog;

import android.content.res.Resources;
import android.support.annotation.StringRes;
import android.view.LayoutInflater;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogProperties;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.modaldialog.ModalDialogViewBinder;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/**
 * A base class for creating, showing and dismissing a modal dialog for a JavaScript popup.
 */
public abstract class JavascriptModalDialog implements ModalDialogView.Controller {
    private static final String TAG = "JSModalDialog";

    private final String mTitle;
    private final String mMessage;
    private final int mPositiveButtonTextId;
    private final int mNegativeButtonTextId;
    private final String mDefaultPromptText;
    private final boolean mShouldShowSuppressCheckBox;

    private ModalDialogManager mModalDialogManager;
    private ModalDialogView mDialogView;
    protected JavascriptDialogCustomView mDialogCustomView;

    protected JavascriptModalDialog(String title, String message, String promptText,
            boolean shouldShowSuppressCheckBox, @StringRes int positiveButtonTextId,
            @StringRes int negativeButtonTextId) {
        mTitle = title;
        mMessage = message;
        mPositiveButtonTextId = positiveButtonTextId;
        mNegativeButtonTextId = negativeButtonTextId;
        mDefaultPromptText = promptText;
        mShouldShowSuppressCheckBox = shouldShowSuppressCheckBox;
    }

    /**
     * Showing a modal dialog for a JavaScript popup with the specified dialog type.
     * @param activity The {@link ChromeActivity} that this dialog is shown upon.
     * @param dialogType The {@link ModalDialogManager.ModalDialogType} of the dialog.
     */
    protected void show(
            ChromeActivity activity, @ModalDialogManager.ModalDialogType int dialogType) {
        mDialogCustomView = (JavascriptDialogCustomView) LayoutInflater.from(activity).inflate(
                R.layout.js_modal_dialog, null);
        mDialogCustomView.setPromptText(mDefaultPromptText);
        mDialogCustomView.setSuppressCheckBoxVisibility(mShouldShowSuppressCheckBox);

        Resources resources = activity.getResources();
        PropertyModel model = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                      .with(ModalDialogProperties.CONTROLLER, this)
                                      .with(ModalDialogProperties.TITLE, mTitle)
                                      .with(ModalDialogProperties.MESSAGE, mMessage)
                                      .with(ModalDialogProperties.CUSTOM_VIEW, mDialogCustomView)
                                      .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                              mPositiveButtonTextId)
                                      .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                              mNegativeButtonTextId)
                                      .with(ModalDialogProperties.TITLE_SCROLLABLE, true)
                                      .build();

        mDialogView = new ModalDialogView(activity);
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        mModalDialogManager = activity.getModalDialogManager();
        mModalDialogManager.showDialog(mDialogView, dialogType);
    }

    /**
     * Dismissing the dialog with the specified reason.
     * @param dismissalCause The specified reason that the dialog is dismissed.
     */
    protected void dismiss(@DialogDismissalCause int dismissalCause) {
        if (mModalDialogManager == null) return;
        mModalDialogManager.dismissDialog(mDialogView, dismissalCause);
    }

    @Override
    public void onClick(@ModalDialogView.ButtonType int buttonType) {
        if (mModalDialogManager == null) return;
        switch (buttonType) {
            case ModalDialogView.ButtonType.POSITIVE:
                mModalDialogManager.dismissDialog(
                        mDialogView, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                break;
            case ModalDialogView.ButtonType.NEGATIVE:
                mModalDialogManager.dismissDialog(
                        mDialogView, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                break;
            default:
                Log.e(TAG, "Unexpected button pressed in dialog: " + buttonType);
        }
    }

    @Override
    public void onDismiss(@DialogDismissalCause int dismissalCause) {
        if (mDialogCustomView == null) return;
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                accept(mDialogCustomView.getPromptText(),
                        mDialogCustomView.isSuppressCheckBoxChecked());
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                cancel(true, mDialogCustomView.isSuppressCheckBoxChecked());
                break;
            case DialogDismissalCause.DISMISSED_BY_NATIVE:
                // We don't need to call native back in this case.
                break;
            default:
                cancel(false, mDialogCustomView.isSuppressCheckBoxChecked());
        }
        mDialogView = null;
        mDialogCustomView = null;
        mModalDialogManager = null;
    }

    /**
     * Sends notification to native that the user accepts the dialog.
     * @param promptResult The text edited by user.
     * @param suppressDialogs Whether the upcoming JavaScript popups should be suppressed.
     */
    protected abstract void accept(String promptResult, boolean suppressDialogs);

    /**
     * Sends notification to native that the user accepts the dialog.
     * @param buttonClicked Whether the dialog is cancelled by user clicking the cancel button.
     * @param suppressDialogs Whether the upcoming JavaScript popups should be suppressed.
     */
    protected abstract void cancel(boolean buttonClicked, boolean suppressDialogs);
}
