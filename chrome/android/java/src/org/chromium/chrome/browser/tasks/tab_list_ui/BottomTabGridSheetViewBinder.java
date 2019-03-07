// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import static org.chromium.chrome.browser.tasks.tab_list_ui.BottomTabGridSheetProperties.ADD_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_list_ui.BottomTabGridSheetProperties.COLLAPSE_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_list_ui.BottomTabGridSheetProperties.CONTENT_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.tab_list_ui.BottomTabGridSheetProperties.HEADER_TITLE;

import android.view.View;
import android.widget.FrameLayout;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for BottomTabGridSheet.
 */
class BottomTabGridSheetViewBinder {
    /**
     * ViewHolder class to get access to all {@link View}s inside the BottomTabGrid.
     */
    public static class ViewHolder {
        public final BottomTabListToolbarView toolbarView;
        public final View contentView;

        ViewHolder(BottomTabListToolbarView toolbarView, View contentView) {
            this.toolbarView = toolbarView;
            this.contentView = contentView;
        }
    }

    /**
     * Binds the given model to the given view, updating the payload in propertyKey.
     * @param model The model to use.
     * @param viewHolder The ViewHolder to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (COLLAPSE_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setLeftButtonOnClickListener(model.get(COLLAPSE_CLICK_LISTENER));
        } else if (ADD_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setRightButtonOnClickListener(model.get(ADD_CLICK_LISTENER));
        } else if (HEADER_TITLE == propertyKey) {
            viewHolder.toolbarView.setTitle(model.get(HEADER_TITLE));
        } else if (CONTENT_TOP_MARGIN == propertyKey) {
            ((FrameLayout.LayoutParams) viewHolder.contentView.getLayoutParams()).topMargin =
                    model.get(CONTENT_TOP_MARGIN);
            viewHolder.contentView.requestLayout();
        }
    }
}
