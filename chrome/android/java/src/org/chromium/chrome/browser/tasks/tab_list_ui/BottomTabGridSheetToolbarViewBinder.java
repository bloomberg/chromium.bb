// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import static org.chromium.chrome.browser.tasks.tab_list_ui.BottomTabGridSheetToolbarProperties.ADD_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_list_ui.BottomTabGridSheetToolbarProperties.COLLAPSE_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_list_ui.BottomTabGridSheetToolbarProperties.HEADER_TITLE;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for BottomTabGridSheetToolbar.
 */
class BottomTabGridSheetToolbarViewBinder {
    /**
     * Binds the given model to the given view, updating the payload in propertyKey.
     * @param model The model to use.
     * @param view The View to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(
            PropertyModel model, BottomTabListToolbarView view, PropertyKey propertyKey) {
        if (COLLAPSE_CLICK_LISTENER == propertyKey) {
            view.setLeftButtonOnClickListener(model.get(COLLAPSE_CLICK_LISTENER));
        } else if (ADD_CLICK_LISTENER == propertyKey) {
            view.setRightButtonOnClickListener(model.get(ADD_CLICK_LISTENER));
        } else if (HEADER_TITLE == propertyKey) {
            view.setTitle(model.get(HEADER_TITLE));
        }
    }
}
