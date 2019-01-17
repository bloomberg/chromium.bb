// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.graphics.RectF;
import android.view.View;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.TouchEventFilterView.Delegate;

import java.util.List;

/**
 * Coordinator responsible for showing a full or partial overlay on top of the web page currently
 * displayed.
 */
class AssistantOverlayCoordinator {
    private final ChromeActivity mActivity;
    private final TouchEventFilterView mTouchEventFilter;
    private final Delegate mTouchEventFilterDelegate;

    AssistantOverlayCoordinator(
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
        // Removes the TouchEventFilter from the ChromeFullscreenManager and GestureListenerManager
        // listeners.
        mTouchEventFilter.deInit();
    }

    /**
     * Enable an overlay that will fully cover the web page.
     */
    public void showFullOverlay() {
        mTouchEventFilter.setFullOverlay(true);
    }

    /**
     * Enable a partial overlay that doesn't cover the portion of the web page whose coordinates
     * are given by {@code coords}.
     */
    public void showPartialOverlay(boolean enabled, List<RectF> coords) {
        mTouchEventFilter.setPartialOverlay(enabled, coords);
    }

    /**
     * Hide any (partial or full) overlay previously shown.
     */
    public void hide() {
        mTouchEventFilter.setFullOverlay(false);
    }
}
