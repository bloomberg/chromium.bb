// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for binding view properties from {@link ModalDialogProperties} to a
 * {@link ModalDialogView}.
 */
public class ModalDialogViewBinder
        implements PropertyModelChangeProcessor
                           .ViewBinder<PropertyModel, ModalDialogView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, ModalDialogView view, PropertyKey propertyKey) {
        if (ModalDialogProperties.TITLE == propertyKey) {
            view.setTitle(model.get(ModalDialogProperties.TITLE));
        } else if (ModalDialogProperties.TITLE_ICON == propertyKey) {
            view.setTitleIcon(model.get(ModalDialogProperties.TITLE_ICON));
        } else if (ModalDialogProperties.MESSAGE == propertyKey) {
            view.setMessage(model.get(ModalDialogProperties.MESSAGE));
        } else if (ModalDialogProperties.CUSTOM_VIEW == propertyKey) {
            view.setCustomView(model.get(ModalDialogProperties.CUSTOM_VIEW));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_TEXT == propertyKey) {
            view.setButtonText(ModalDialogView.ButtonType.POSITIVE,
                    model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_DISABLED == propertyKey) {
            view.setButtonEnabled(ModalDialogView.ButtonType.POSITIVE,
                    !model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_TEXT == propertyKey) {
            view.setButtonText(ModalDialogView.ButtonType.NEGATIVE,
                    model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_DISABLED == propertyKey) {
            view.setButtonEnabled(ModalDialogView.ButtonType.NEGATIVE,
                    !model.get(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED));
        } else if (ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE == propertyKey) {
            view.setCancelOnTouchOutside(model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
        } else if (ModalDialogProperties.CONTENT_DESCRIPTION == propertyKey) {
            view.setContentDescription(model.get(ModalDialogProperties.CONTENT_DESCRIPTION));
        } else if (ModalDialogProperties.TITLE_SCROLLABLE == propertyKey) {
            view.setTitleScrollable(model.get(ModalDialogProperties.TITLE_SCROLLABLE));
        } else if (ModalDialogProperties.CONTROLLER == propertyKey) {
            view.setController(model.get(ModalDialogProperties.CONTROLLER));
        } else {
            assert false : "Unhandled property detected in ModalDialogViewBinder!";
        }
    }
}
