// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import static org.chromium.chrome.browser.tasks.tab_list_ui.TabStripToolbarViewProperties.ADD_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabStripToolbarViewProperties.EXPAND_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabStripToolbarViewProperties.IS_MAIN_CONTENT_VISIBLE;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for BottomTabStripToolbar.
 */
class BottomTabStripToolbarViewBinder {
    /**
     * Binds the given model to the given view, updating the payload in propertyKey.
     *
     * @param model       The model to use.
     * @param view        The view to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(
            PropertyModel model, BottomTabListToolbarView view, PropertyKey propertyKey) {
        if (EXPAND_CLICK_LISTENER == propertyKey) {
            view.setLeftButtonOnClickListener(model.get(EXPAND_CLICK_LISTENER));
        } else if (ADD_CLICK_LISTENER == propertyKey) {
            view.setRightButtonOnClickListener(model.get(ADD_CLICK_LISTENER));
        } else if (IS_MAIN_CONTENT_VISIBLE == propertyKey) {
            view.setMainContentVisibility(model.get(IS_MAIN_CONTENT_VISIBLE));
        }
    }
}
