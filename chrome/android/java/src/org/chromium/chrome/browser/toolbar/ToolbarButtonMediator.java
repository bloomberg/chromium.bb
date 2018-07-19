// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.modelutil.PropertyModel;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the button, and updating
 * the model accordingly.
 */
class ToolbarButtonMediator implements OverviewModeObserver {
    /** The model for the button that holds all of its state. */
    private PropertyModel mModel;

    /** The overview mode manager. */
    private OverviewModeBehavior mOverviewModeBehavior;

    private @ToolbarButtonCoordinator.ButtonVisibility int mButtonVisibility;

    /**
     * Build a new mediator that handles events from outside the button.
     * @param model The {@link PropertyModel} that holds all the state for the button.
     */
    public ToolbarButtonMediator(PropertyModel model) {
        mModel = model;
    }

    /**
     * Clean up anything that needs to be when the button is destroyed.
     */
    public void destroy() {
        mOverviewModeBehavior.removeOverviewModeObserver(this);
    }

    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {
        mModel.setValue(ToolbarButtonProperties.IS_VISIBLE,
                mButtonVisibility == ToolbarButtonCoordinator.ButtonVisibility.TAB_SWITCHER_MODE);
    }

    @Override
    public void onOverviewModeFinishedShowing() {}

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
        mModel.setValue(ToolbarButtonProperties.IS_VISIBLE,
                mButtonVisibility == ToolbarButtonCoordinator.ButtonVisibility.BROWSING_MODE);
    }

    @Override
    public void onOverviewModeFinishedHiding() {}

    public void setButtonListeners(
            OnClickListener onClickListener, OnLongClickListener onLongClickListener) {
        mModel.setValue(ToolbarButtonProperties.ON_CLICK_LISTENER, onClickListener);
        mModel.setValue(ToolbarButtonProperties.ON_LONG_CLICK_LISTENER, onLongClickListener);
    }

    public void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior,
            @ToolbarButtonCoordinator.ButtonVisibility int buttonVisibility) {
        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeBehavior.addOverviewModeObserver(this);
        mButtonVisibility = buttonVisibility;
    }
}
