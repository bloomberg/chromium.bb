// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.permissions;

import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.permissions.PermissionDialogDelegate;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class creates the model for permission dialog in touchless mode.
 */
public class TouchlessPermissionDialogModel {
    public static PropertyModel getModel(
            ModalDialogProperties.Controller controller, PermissionDialogDelegate delegate) {
        Resources resources = delegate.getTab().getActivity().getResources();
        TouchlessDialogProperties.ActionNames names = new TouchlessDialogProperties.ActionNames();
        names.cancel = R.string.cancel;
        names.select = R.string.select;
        names.alt = 0;
        Drawable icon =
                ApiCompatibilityUtils.getDrawable(resources, delegate.getDrawableId()).mutate();
        icon.setColorFilter(new PorterDuffColorFilter(
                resources.getColor(R.color.modern_grey_700), PorterDuff.Mode.SRC_ATOP));
        String messageText = delegate.getMessageText();
        assert !TextUtils.isEmpty(messageText);

        PropertyModel model =
                new PropertyModel.Builder(TouchlessDialogProperties.ALL_DIALOG_KEYS)
                        .with(TouchlessDialogProperties.IS_FULLSCREEN, false)
                        .with(TouchlessDialogProperties.PRIORITY,
                                TouchlessDialogProperties.Priority.HIGH)
                        .with(TouchlessDialogProperties.ACTION_NAMES, names)
                        .with(TouchlessDialogProperties.ALT_ACTION, null)
                        .with(ModalDialogProperties.MESSAGE, messageText)
                        .with(ModalDialogProperties.TITLE, delegate.getTitleText())
                        .with(ModalDialogProperties.TITLE_ICON, icon)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true)
                        .with(ModalDialogProperties.CONTENT_DESCRIPTION, delegate.getMessageText())
                        .build();

        PropertyModel[] items = new PropertyModel[]{
                new PropertyModel
                        .Builder(TouchlessDialogProperties.DialogListItemProperties.ALL_KEYS)
                        .with(TouchlessDialogProperties.DialogListItemProperties.TEXT,
                                delegate.getPrimaryButtonText())
                        .with(TouchlessDialogProperties.DialogListItemProperties.CLICK_LISTENER,
                                (v) -> controller.onClick(model,
                                        ModalDialogProperties.ButtonType.POSITIVE))
                        .with(TouchlessDialogProperties.DialogListItemProperties.ICON,
                                ApiCompatibilityUtils.getDrawable(
                                        resources, R.drawable.ic_check_circle))
                        .build(),
                new PropertyModel
                        .Builder(TouchlessDialogProperties.DialogListItemProperties.ALL_KEYS)
                        .with(TouchlessDialogProperties.DialogListItemProperties.TEXT,
                                delegate.getSecondaryButtonText())
                        .with(TouchlessDialogProperties.DialogListItemProperties.CLICK_LISTENER,
                                (v) -> controller.onClick(model,
                                        ModalDialogProperties.ButtonType.NEGATIVE))
                        .with(TouchlessDialogProperties.DialogListItemProperties.ICON,
                                ApiCompatibilityUtils.getDrawable(
                                        resources, R.drawable.ic_cancel_circle))
                        .build()};

        model.set(TouchlessDialogProperties.LIST_MODELS, items);
        model.set(TouchlessDialogProperties.CANCEL_ACTION,
                (v) -> delegate.getTab().getActivity().getModalDialogManager().dismissDialog(model,
                        DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE));
        return model;
    }
}
