// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;

import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.modelutil.PropertyModel;

/**
 * All of the state for the bottom toolbar, updated by the {@link BottomToolbarController}.
 */
public class BottomToolbarModel extends PropertyModel {
    /** The Y offset of the view in px. */
    public static final IntPropertyKey Y_OFFSET = new IntPropertyKey();

    /** The visibility of the Android view version of the toolbar. */
    public static final IntPropertyKey ANDROID_VIEW_VISIBILITY = new IntPropertyKey();

    /** The click listener for the search accelerator. */
    public static final ObjectPropertyKey<OnClickListener> SEARCH_ACCELERATOR_LISTENER =
            new ObjectPropertyKey<>();

    /** The touch listener for the home button. */
    public static final ObjectPropertyKey<OnClickListener> HOME_BUTTON_LISTENER =
            new ObjectPropertyKey<>();

    /** The touch listener for the menu button. */
    public static final ObjectPropertyKey<OnTouchListener> MENU_BUTTON_LISTENER =
            new ObjectPropertyKey<>();

    /** A {@link LayoutManager} to attach overlays to. */
    public static final ObjectPropertyKey<LayoutManager> LAYOUT_MANAGER = new ObjectPropertyKey<>();

    /** Whether or not the search accelerator is visible. */
    public static final BooleanPropertyKey SEARCH_ACCELERATOR_VISIBLE = new BooleanPropertyKey();

    /** Default constructor. */
    public BottomToolbarModel() {
        super(Y_OFFSET, ANDROID_VIEW_VISIBILITY, SEARCH_ACCELERATOR_LISTENER, HOME_BUTTON_LISTENER,
                MENU_BUTTON_LISTENER, LAYOUT_MANAGER, SEARCH_ACCELERATOR_VISIBLE);
    }
}
