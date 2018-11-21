// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modaldialog.ModalDialogProperties;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.modaldialog.ModalDialogViewBinder;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/** Class holding a {@link ModalDialogView} that is lazily created after all the data is made
 * available.
 */
public class PasswordGenerationDialogViewHolder {
    private ModalDialogView mView;
    private String mGeneratedPassword;
    private String mSaveExplanationText;
    private Context mContext;
    private ModalDialogView.Controller mController;
    // TODO(crbug.com/835234): Make the generated password editable.
    private TextView mGeneratedPasswordTextView;
    private TextView mSaveExplantaionTextView;

    public PasswordGenerationDialogViewHolder(Context context) {
        mContext = context;
    }

    public void setGeneratedPassword(String generatedPassword) {
        mGeneratedPassword = generatedPassword;
    }

    public void setSaveExplanationText(String saveExplanationText) {
        mSaveExplanationText = saveExplanationText;
    }

    public void setController(ModalDialogView.Controller controller) {
        mController = controller;
    }

    public void initializeView() {
        View customView =
                LayoutInflater.from(mContext).inflate(R.layout.password_generation_dialog, null);
        mGeneratedPasswordTextView = customView.findViewById(R.id.generated_password);
        mGeneratedPasswordTextView.setText(mGeneratedPassword);
        mSaveExplantaionTextView = customView.findViewById(R.id.generation_save_explanation);
        mSaveExplantaionTextView.setText(mSaveExplanationText);

        Resources resources = mContext.getResources();
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mController)
                        .with(ModalDialogProperties.TITLE, resources,
                                R.string.password_generation_dialog_title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.password_generation_dialog_use_password_button)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.password_generation_dialog_cancel_button)
                        .build();

        mView = new ModalDialogView(mContext);
        PropertyModelChangeProcessor.create(model, mView, new ModalDialogViewBinder());
    }

    @Nullable
    public ModalDialogView getView() {
        return mView;
    }
}
