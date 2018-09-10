// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.toolbar.ToolbarButtonSlotData.ToolbarButtonData;
import org.chromium.ui.resources.ResourceManager;

/**
 * All of the state for the bottom toolbar, updated by the {@link BottomToolbarCoordinator}.
 */
public class BottomToolbarModel extends PropertyModel {
    /** The Y offset of the view in px. */
    public static final WritableIntPropertyKey Y_OFFSET = new WritableIntPropertyKey();

    /** Whether the Android view version of the toolbar is visible. */
    public static final WritableBooleanPropertyKey ANDROID_VIEW_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether the composited version of the toolbar is visible. */
    public static final WritableBooleanPropertyKey COMPOSITED_VIEW_VISIBLE =
            new WritableBooleanPropertyKey();

    /** A {@link LayoutManager} to attach overlays to. */
    public static final WritableObjectPropertyKey<LayoutManager> LAYOUT_MANAGER =
            new WritableObjectPropertyKey<>();

    /** The browser's {@link ToolbarSwipeLayout}. */
    public static final WritableObjectPropertyKey<ToolbarSwipeLayout> TOOLBAR_SWIPE_LAYOUT =
            new WritableObjectPropertyKey<>();

    /** A {@link ResourceManager} for loading textures into the compositor. */
    public static final WritableObjectPropertyKey<ResourceManager> RESOURCE_MANAGER =
            new WritableObjectPropertyKey<>();

    /** A handler for swipe events on the toolbar. */
    public static final WritableObjectPropertyKey<EdgeSwipeHandler> TOOLBAR_SWIPE_HANDLER =
            new WritableObjectPropertyKey<>();

    /** Data used to show the first button. */
    public static final WritableObjectPropertyKey<ToolbarButtonData> FIRST_BUTTON_DATA =
            new WritableObjectPropertyKey<>();

    /** Data used to show the second button. */
    public static final WritableObjectPropertyKey<ToolbarButtonData> SECOND_BUTTON_DATA =
            new WritableObjectPropertyKey<>();

    /** Primary color of bottom toolbar. */
    public static final WritableIntPropertyKey PRIMARY_COLOR = new WritableIntPropertyKey();

    /** Default constructor. */
    public BottomToolbarModel() {
        super(Y_OFFSET, ANDROID_VIEW_VISIBLE, COMPOSITED_VIEW_VISIBLE, LAYOUT_MANAGER,
                TOOLBAR_SWIPE_LAYOUT, RESOURCE_MANAGER, TOOLBAR_SWIPE_HANDLER, FIRST_BUTTON_DATA,
                SECOND_BUTTON_DATA, PRIMARY_COLOR);
    }
}
