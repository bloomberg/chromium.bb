// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.graphics.drawable.Drawable;
import android.support.annotation.ColorRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.chrome.browser.omnibox.status.StatusView.NavigationButtonType;
import org.chromium.chrome.browser.widget.TintedDrawable;

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
        } else if (StatusProperties.NAVIGATION_BUTTON_TYPE.equals(propertyKey)
                || StatusProperties.ICON_TINT_COLOR_RES.equals(propertyKey)) {
            updateNavigationButtonType(view, model.get(StatusProperties.NAVIGATION_BUTTON_TYPE),
                    model.get(StatusProperties.ICON_TINT_COLOR_RES));
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

    private static void updateNavigationButtonType(
            StatusView view, @NavigationButtonType int buttonType, @ColorRes int tintColor) {
        Drawable image = null;

        switch (buttonType) {
            case NavigationButtonType.PAGE:
                image = TintedDrawable.constructTintedDrawable(
                        view.getContext(), R.drawable.ic_omnibox_page, tintColor);
                break;
            case NavigationButtonType.MAGNIFIER:
                image = TintedDrawable.constructTintedDrawable(
                        view.getContext(), R.drawable.omnibox_search, tintColor);
                break;
            case NavigationButtonType.EMPTY:
                break;
            default:
                assert false;
        }
        view.setNavigationIcon(image);
    }
}
