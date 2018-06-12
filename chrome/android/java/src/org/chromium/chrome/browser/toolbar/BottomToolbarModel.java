// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;

import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.modelutil.PropertyObservable;

/**
 * All of the state for the bottom toolbar, updated by the {@link BottomToolbarController}.
 */
public class BottomToolbarModel extends PropertyObservable<BottomToolbarModel.PropertyKey> {
    /** The different properties that can change on the bottom toolbar. */
    public static class PropertyKey {
        public static final PropertyKey Y_OFFSET = new PropertyKey();
        public static final PropertyKey ANDROID_VIEW_VISIBILITY = new PropertyKey();
        public static final PropertyKey SEARCH_ACCELERATOR_LISTENER = new PropertyKey();
        public static final PropertyKey MENU_BUTTON_LISTENER = new PropertyKey();
        public static final PropertyKey LAYOUT_MANAGER = new PropertyKey();

        private PropertyKey() {}
    }

    /** The Y offset of the view in px. */
    private int mYOffsetPx;

    /** The visibility of the Android view version of the toolbar. */
    private int mAndroidViewVisibility;

    /** The click listener for the search accelerator. */
    private OnClickListener mSearchAcceleratorListener;

    /** The touch listener for the menu button. */
    private OnTouchListener mMenuButtonListener;

    /** A {@link LayoutManager} to attach overlays to. */
    private LayoutManager mLayoutManager;

    /** Default constructor. */
    public BottomToolbarModel() {}

    /**
     * @param offsetPx The current Y offset in px.
     */
    public void setYOffset(int offsetPx) {
        mYOffsetPx = offsetPx;
        notifyPropertyChanged(PropertyKey.Y_OFFSET);
    }

    /**
     * @return The current Y offset in px.
     */
    public int getYOffset() {
        return mYOffsetPx;
    }

    /**
     * @param visibility The visibility of the Android view version of the toolbar.
     */
    public void setAndroidViewVisibility(int visibility) {
        mAndroidViewVisibility = visibility;
        notifyPropertyChanged(PropertyKey.ANDROID_VIEW_VISIBILITY);
    }

    /**
     * @return The visibility of the Android view version of the toolbar.
     */
    public int getAndroidViewVisibility() {
        return mAndroidViewVisibility;
    }

    /**
     * @param listener The listener for the search accelerator.
     */
    public void setSearchAcceleratorListener(OnClickListener listener) {
        mSearchAcceleratorListener = listener;
        notifyPropertyChanged(PropertyKey.SEARCH_ACCELERATOR_LISTENER);
    }

    /**
     * @return The listener for the search accelerator.
     */
    public OnClickListener getSearchAcceleratorListener() {
        return mSearchAcceleratorListener;
    }

    /**
     * @param listener The listener for the menu button.
     */
    public void setMenuButtonListener(OnTouchListener listener) {
        mMenuButtonListener = listener;
        notifyPropertyChanged(PropertyKey.MENU_BUTTON_LISTENER);
    }

    /**
     * @return The listener for the menu button.
     */
    public OnTouchListener getMenuButtonListener() {
        return mMenuButtonListener;
    }

    /**
     * @param layoutManager A {@link LayoutManager} to attach overlays to.
     */
    public void setLayoutManager(LayoutManager layoutManager) {
        mLayoutManager = layoutManager;
        notifyPropertyChanged(PropertyKey.LAYOUT_MANAGER);
    }

    /**
     * @return A {@link LayoutManager} to attach overlays to.
     */
    public LayoutManager getLayoutManager() {
        return mLayoutManager;
    }
}
