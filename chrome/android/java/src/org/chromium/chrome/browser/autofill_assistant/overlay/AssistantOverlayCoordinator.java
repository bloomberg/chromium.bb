// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.view.View;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.overlay.TouchEventFilterView.Delegate;

/**
 * Coordinator responsible for showing a full or partial overlay on top of the web page currently
 * displayed.
 */
public class AssistantOverlayCoordinator {
    private final ChromeActivity mActivity;
    private final TouchEventFilterView mTouchEventFilter;
    private final Delegate mTouchEventFilterDelegate;

    public AssistantOverlayCoordinator(
            ChromeActivity activity, View assistantView, Delegate touchEventFilterDelegate) {
        mActivity = activity;
        mTouchEventFilter = assistantView.findViewById(
                org.chromium.chrome.autofill_assistant.R.id.touch_event_filter);
        mTouchEventFilterDelegate = touchEventFilterDelegate;

        mTouchEventFilter.init(mTouchEventFilterDelegate, mActivity.getFullscreenManager(),
                mActivity.getActivityTab().getWebContents(), mActivity.getCompositorViewHolder());
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
    public void setState(AssistantOverlayState state) {
        if (state.isFull() && !mActivity.isViewObscuringAllTabs()) {
            mActivity.addViewObscuringAllTabs(mTouchEventFilter);
        }

        if (!state.isFull() && mActivity.isViewObscuringAllTabs()) {
            mActivity.removeViewObscuringAllTabs(mTouchEventFilter);
        }

        mTouchEventFilter.setState(state);
    }
}
