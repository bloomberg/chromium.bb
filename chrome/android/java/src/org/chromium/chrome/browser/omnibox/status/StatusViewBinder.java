// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * StatusViewBinder observes StatusModel changes and triggers StatusView updates.
 */
class StatusViewBinder implements ViewBinder<PropertyModel, StatusView, PropertyKey> {
    public StatusViewBinder() {}

    @Override
    public void bind(PropertyModel model, StatusView view, PropertyKey propertyKey) {
        if (StatusProperties.ANIMATIONS_ENABLED.equals(propertyKey)) {
            view.setAnimationsEnabled(model.get(StatusProperties.ANIMATIONS_ENABLED));
        } else if (StatusProperties.STATUS_BUTTON_TYPE.equals(propertyKey)) {
            view.setStatusButtonType(model.get(StatusProperties.STATUS_BUTTON_TYPE));
        } else if (StatusProperties.NAVIGATION_ICON_RES.equals(propertyKey)) {
            view.setNavigationIcon(model.get(StatusProperties.NAVIGATION_ICON_RES));
        } else if (StatusProperties.NAVIGATION_ICON_TINT_RES.equals(propertyKey)) {
            view.setNavigationIconTint(model.get(StatusProperties.NAVIGATION_ICON_TINT_RES));
        } else if (StatusProperties.SECURITY_ICON_RES.equals(propertyKey)) {
            view.setSecurityIcon(model.get(StatusProperties.SECURITY_ICON_RES));
        } else if (StatusProperties.SECURITY_ICON_TINT_RES.equals(propertyKey)) {
            view.setSecurityIconTint(model.get(StatusProperties.SECURITY_ICON_TINT_RES));
        } else if (StatusProperties.SECURITY_ICON_DESCRIPTION_RES.equals(propertyKey)) {
            view.setSecurityIconDescription(
                    model.get(StatusProperties.SECURITY_ICON_DESCRIPTION_RES));
        } else if (StatusProperties.SEPARATOR_COLOR_RES.equals(propertyKey)) {
            view.setSeparatorColor(model.get(StatusProperties.SEPARATOR_COLOR_RES));
        } else if (StatusProperties.STATUS_CLICK_LISTENER.equals(propertyKey)) {
            view.setStatusClickListener(model.get(StatusProperties.STATUS_CLICK_LISTENER));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES.equals(propertyKey)) {
            view.setVerboseStatusTextColor(
                    model.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES.equals(propertyKey)) {
            view.setVerboseStatusTextContent(
                    model.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE.equals(propertyKey)) {
            view.setVerboseStatusTextVisible(
                    model.get(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_WIDTH.equals(propertyKey)) {
            view.setVerboseStatusTextWidth(model.get(StatusProperties.VERBOSE_STATUS_TEXT_WIDTH));
        } else {
            assert false : "Unhandled property update";
        }
    }
}
