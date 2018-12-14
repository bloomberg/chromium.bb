// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.support.annotation.ColorRes;

import org.chromium.base.ApiCompatibilityUtils;
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
        if (StatusProperties.USE_DARK_COLORS.equals(propertyKey)
                || StatusProperties.PAGE_IS_PREVIEW.equals(propertyKey)
                || StatusProperties.PAGE_IS_OFFLINE.equals(propertyKey)) {
            updateTextColors(view, model.get(StatusProperties.USE_DARK_COLORS),
                    model.get(StatusProperties.PAGE_IS_PREVIEW),
                    model.get(StatusProperties.PAGE_IS_OFFLINE));
            updateNavigationButtonType(view, model.get(StatusProperties.NAVIGATION_BUTTON_TYPE),
                    model.get(StatusProperties.USE_DARK_COLORS));
        } else if (StatusProperties.NAVIGATION_BUTTON_TYPE.equals(propertyKey)) {
            updateNavigationButtonType(view, model.get(StatusProperties.NAVIGATION_BUTTON_TYPE),
                    model.get(StatusProperties.USE_DARK_COLORS));
        } else {
            assert false : "Unhandled property update";
        }
    }

    private static void updateTextColors(
            StatusView view, boolean useDarkColors, boolean pageIsPreview, boolean pageIsOffline) {
        Resources res = view.getResources();

        @ColorRes
        int separatorColor = ApiCompatibilityUtils.getColor(res,
                useDarkColors ? R.color.locationbar_status_separator_color
                              : R.color.locationbar_status_separator_color_light);

        @ColorRes
        int textColor = 0;
        if (pageIsPreview) {
            // There will never be a Preview in Incognito and the site theme color is not used. So
            // ignore useDarkColors.
            textColor =
                    ApiCompatibilityUtils.getColor(res, R.color.locationbar_status_preview_color);
        } else if (pageIsOffline) {
            textColor = ApiCompatibilityUtils.getColor(res,
                    useDarkColors ? R.color.locationbar_status_offline_color
                                  : R.color.locationbar_status_offline_color_light);
        }

        view.updateVisualTheme(textColor, separatorColor);
    }

    private static void updateNavigationButtonType(
            StatusView view, @NavigationButtonType int buttonType, boolean useDarkColors) {
        @ColorRes
        int tint = useDarkColors ? R.color.dark_mode_tint : R.color.light_mode_tint;
        Drawable image = null;

        switch (buttonType) {
            case NavigationButtonType.PAGE:
                image = TintedDrawable.constructTintedDrawable(
                        view.getContext(), R.drawable.ic_omnibox_page, tint);
                break;
            case NavigationButtonType.MAGNIFIER:
                image = TintedDrawable.constructTintedDrawable(
                        view.getContext(), R.drawable.omnibox_search, tint);
                break;
            case NavigationButtonType.EMPTY:
                break;
            default:
                assert false;
        }
        view.setNavigationIcon(image);
    }
}
