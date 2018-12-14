// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.omnibox.status.StatusView.NavigationButtonType;

/**
 * Contains the controller logic of the Status component.
 */
class StatusMediator {
    private final PropertyModel mModel;

    public StatusMediator(PropertyModel model) {
        mModel = model;
    }

    /**
     * Specify navigation button image type.
     */
    public void setNavigationButtonType(@NavigationButtonType int buttonType) {
        mModel.set(StatusProperties.NAVIGATION_BUTTON_TYPE, buttonType);
    }

    /**
     * Specify whether displayed page is an offline page.
     */
    public void setPageIsOffline(boolean pageIsOffline) {
        mModel.set(StatusProperties.PAGE_IS_OFFLINE, pageIsOffline);
    }

    /**
     * Specify whether displayed page is a preview page.
     */
    public void setPageIsPreview(boolean pageIsPreview) {
        mModel.set(StatusProperties.PAGE_IS_PREVIEW, pageIsPreview);
    }

    /**
     * Toggle between dark and light UI color theme.
     */
    public void setUseDarkColors(boolean useDarkColors) {
        mModel.set(StatusProperties.USE_DARK_COLORS, useDarkColors);
    }
}
