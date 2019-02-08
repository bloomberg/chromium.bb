// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.VISIBILITY_LISTENER;

import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class TabGridContainerViewBinder {
    /**
     * Bind the given model to the given view, updating the payload in propertyKey.
     * @param model The model to use.
     * @param view The View to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(
            PropertyModel model, TabListRecyclerView view, PropertyKey propertyKey) {
        if (IS_VISIBLE == propertyKey) {
            if (model.get(IS_VISIBLE)) {
                view.startShowing();
            } else {
                view.startHiding();
            }
        } else if (IS_INCOGNITO == propertyKey) {
            view.setBackgroundColor(
                    ColorUtils.getDefaultThemeColor(view.getResources(), model.get(IS_INCOGNITO)));
        } else if (VISIBILITY_LISTENER == propertyKey) {
            view.setVisibilityListener(model.get(VISIBILITY_LISTENER));
        }
    }
}
