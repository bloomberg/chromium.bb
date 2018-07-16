// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View.OnClickListener;

import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

/**
 * All of the state for the bottom toolbar, updated by the {@link BottomToolbarCoordinator}.
 */
public class BottomToolbarModel extends PropertyModel {
    /** The Y offset of the view in px. */
    public static final IntPropertyKey Y_OFFSET = new IntPropertyKey();

    /** Whether the Android view version of the toolbar is visible. */
    public static final BooleanPropertyKey ANDROID_VIEW_VISIBLE = new BooleanPropertyKey();

    /** Whether the composited version of the toolbar is visible. */
    public static final BooleanPropertyKey COMPOSITED_VIEW_VISIBLE = new BooleanPropertyKey();

    /** The click listener for the search accelerator. */
    public static final ObjectPropertyKey<OnClickListener> SEARCH_ACCELERATOR_LISTENER =
            new ObjectPropertyKey<>();

    /** A {@link LayoutManager} to attach overlays to. */
    public static final ObjectPropertyKey<LayoutManager> LAYOUT_MANAGER = new ObjectPropertyKey<>();

    /** The browser's {@link ToolbarSwipeLayout}. */
    public static final ObjectPropertyKey<ToolbarSwipeLayout> TOOLBAR_SWIPE_LAYOUT =
            new ObjectPropertyKey<>();

    /** A {@link ResourceManager} for loading textures into the compositor. */
    public static final ObjectPropertyKey<ResourceManager> RESOURCE_MANAGER =
            new ObjectPropertyKey<>();

    /** Whether or not the search accelerator is visible. */
    public static final BooleanPropertyKey SEARCH_ACCELERATOR_VISIBLE = new BooleanPropertyKey();

    /** A handler for swipe events on the toolbar. */
    public static final ObjectPropertyKey<EdgeSwipeHandler> TOOLBAR_SWIPE_HANDLER =
            new ObjectPropertyKey<>();

    /** Default constructor. */
    public BottomToolbarModel() {
        super(Y_OFFSET, ANDROID_VIEW_VISIBLE, COMPOSITED_VIEW_VISIBLE, SEARCH_ACCELERATOR_LISTENER,
                LAYOUT_MANAGER, TOOLBAR_SWIPE_LAYOUT, RESOURCE_MANAGER, SEARCH_ACCELERATOR_VISIBLE,
                TOOLBAR_SWIPE_HANDLER);
    }
}
