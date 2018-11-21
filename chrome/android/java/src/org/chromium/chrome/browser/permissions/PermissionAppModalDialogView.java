// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.content.Context;
import android.support.v4.widget.TextViewCompat;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modaldialog.ModalDialogProperties;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.modaldialog.ModalDialogViewBinder;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/**
 * The Permission dialog that is app modal.
 */
class PermissionAppModalDialogView {
    private final ModalDialogView mDialogView;

    PermissionAppModalDialogView(
            ModalDialogView.Controller controller, PermissionDialogDelegate delegate) {
        Context context = delegate.getTab().getActivity();
        LayoutInflater inflater = LayoutInflater.from(context);
        View customView = inflater.inflate(R.layout.permission_dialog, null);

        String messageText = delegate.getMessageText();
        assert !TextUtils.isEmpty(messageText);

        TextView messageTextView = customView.findViewById(R.id.text);
        messageTextView.setText(messageText);
        TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(
                messageTextView, delegate.getDrawableId(), 0, 0, 0);

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                delegate.getPrimaryButtonText())
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                delegate.getSecondaryButtonText())
                        .with(ModalDialogProperties.CONTENT_DESCRIPTION, delegate.getMessageText())
                        .build();

        mDialogView = new ModalDialogView(context);
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());
    }

    /**
     * @return The {@link ModalDialogView} for the permission dialog.
     */
    ModalDialogView getDialogView() {
        return mDialogView;
    }
}
