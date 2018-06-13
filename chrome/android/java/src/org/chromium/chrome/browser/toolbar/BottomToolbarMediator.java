// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.Resources;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the bottom toolbar, and updating
 * the model accordingly.
 */
class BottomToolbarMediator implements FullscreenListener {
    /** The model for the bottom toolbar that holds all of its state. */
    private BottomToolbarModel mModel;

    /** The fullscreen manager to observe browser controls events. */
    private ChromeFullscreenManager mFullscreenManager;

    /**
     * Build a new mediator that handles events from outside the bottom toolbar.
     * @param model The {@link BottomToolbarModel} that holds all the state for the bottom toolbar.
     * @param fullscreenManager A {@link ChromeFullscreenManager} for events related to the browser
     *                          controls.
     * @param resources Android {@link Resources} to pull dimensions from.
     */
    public BottomToolbarMediator(BottomToolbarModel model,
            ChromeFullscreenManager fullscreenManager, Resources resources) {
        mModel = model;
        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addListener(this);

        // Notify the fullscreen manager that the bottom controls now have a height.
        fullscreenManager.setBottomControlsHeight(
                resources.getDimensionPixelOffset(R.dimen.bottom_toolbar_height));
        fullscreenManager.updateViewportSize();
    }

    /**
     * Clean up anything that needs to be when the bottom toolbar is destroyed.
     */
    public void destroy() {
        mFullscreenManager.removeListener(this);
    }

    @Override
    public void onContentOffsetChanged(float offset) {}

    @Override
    public void onControlsOffsetChanged(float topOffset, float bottomOffset, boolean needsAnimate) {
        mModel.setValue(BottomToolbarModel.Y_OFFSET, (int) bottomOffset);
        if (bottomOffset > 0) {
            mModel.setValue(BottomToolbarModel.ANDROID_VIEW_VISIBILITY, View.INVISIBLE);
        } else {
            mModel.setValue(BottomToolbarModel.ANDROID_VIEW_VISIBILITY, View.VISIBLE);
        }
    }

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {}

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {}

    public void setButtonListeners(
            OnClickListener searchAcceleratorListener, OnTouchListener menuButtonListener) {
        mModel.setValue(BottomToolbarModel.SEARCH_ACCELERATOR_LISTENER, searchAcceleratorListener);
        mModel.setValue(BottomToolbarModel.MENU_BUTTON_LISTENER, menuButtonListener);
    }

    public void setLayoutManager(LayoutManager layoutManager) {
        mModel.setValue(BottomToolbarModel.LAYOUT_MANAGER, layoutManager);
    }
}
