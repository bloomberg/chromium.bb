// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.TOP_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.VISIBILITY_LISTENER;

import android.widget.FrameLayout;

import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for TabGridContainer.
 */
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
                view.startShowing(model.get(ANIMATE_VISIBILITY_CHANGES));
            } else {
                view.startHiding(model.get(ANIMATE_VISIBILITY_CHANGES));
            }
        } else if (IS_INCOGNITO == propertyKey) {
            view.setBackgroundColor(
                    ColorUtils.getDefaultThemeColor(view.getResources(), model.get(IS_INCOGNITO)));
        } else if (VISIBILITY_LISTENER == propertyKey) {
            view.setVisibilityListener(model.get(VISIBILITY_LISTENER));
        } else if (INITIAL_SCROLL_INDEX == propertyKey) {
            view.scrollToPosition(model.get(INITIAL_SCROLL_INDEX));
        } else if (TOP_CONTROLS_HEIGHT == propertyKey) {
            FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) view.getLayoutParams();
            params.topMargin = model.get(TOP_CONTROLS_HEIGHT);
            view.requestLayout();
        } else if (BOTTOM_CONTROLS_HEIGHT == propertyKey) {
            FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) view.getLayoutParams();
            params.bottomMargin = model.get(BOTTOM_CONTROLS_HEIGHT);
            view.requestLayout();
        }
    }
}
