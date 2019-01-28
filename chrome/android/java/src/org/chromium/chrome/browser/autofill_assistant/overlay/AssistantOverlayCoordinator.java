// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.view.View;

import org.chromium.chrome.browser.ChromeActivity;

/**
 * Coordinator responsible for showing a full or partial overlay on top of the web page currently
 * displayed.
 */
public class AssistantOverlayCoordinator {
    private final ChromeActivity mActivity;
    private final TouchEventFilterView mTouchEventFilter;

    public AssistantOverlayCoordinator(
            ChromeActivity activity, View assistantView, AssistantOverlayModel model) {
        mActivity = activity;
        mTouchEventFilter = assistantView.findViewById(
                org.chromium.chrome.autofill_assistant.R.id.touch_event_filter);

        mTouchEventFilter.init(mActivity.getFullscreenManager(),
                mActivity.getActivityTab().getWebContents(), mActivity.getCompositorViewHolder());

        // Listen for changes in the state.
        // TODO(crbug.com/806868): Bind model to view through a ViewBinder instead.
        model.addObserver((source, propertyKey) -> {
            if (AssistantOverlayModel.STATE == propertyKey) {
                AssistantOverlayState newState = model.get(AssistantOverlayModel.STATE);
                setState(newState != null ? newState : AssistantOverlayState.hidden());
            } else if (AssistantOverlayModel.DELEGATE == propertyKey) {
                mTouchEventFilter.setDelegate(model.get(AssistantOverlayModel.DELEGATE));
            }
        });
    }

    /**
     * Destroy this coordinator.
     */
    public void destroy() {
        if (mActivity.isViewObscuringAllTabs())
            mActivity.removeViewObscuringAllTabs(mTouchEventFilter);

        // Removes the TouchEventFilter from the ChromeFullscreenManager and GestureListenerManager
        // listeners.
        mTouchEventFilter.deInit();
    }

    /**
     * Set the overlay state.
     */
    private void setState(AssistantOverlayState state) {
        if (state.isFull() && !mActivity.isViewObscuringAllTabs()) {
            mActivity.addViewObscuringAllTabs(mTouchEventFilter);
        }

        if (!state.isFull() && mActivity.isViewObscuringAllTabs()) {
            mActivity.removeViewObscuringAllTabs(mTouchEventFilter);
        }

        mTouchEventFilter.setState(state);
    }
}
