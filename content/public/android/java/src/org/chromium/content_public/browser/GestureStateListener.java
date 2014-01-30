// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * A class that is notified of events and state changes related to gesture processing from
 * the ContentViewCore.
 */
public class GestureStateListener {
    /**
     * Called when the pinch gesture starts.
     */
    public void onPinchGestureStart() {}

    /**
     * Called when the pinch gesture ends.
     */
    public void onPinchGestureEnd() {}

    /**
     * Called when the fling gesture is sent.
     */
    public void onFlingStartGesture(int vx, int vy, int scrollOffsetY, int scrollExtentY) {}

    /**
     * Called when the fling cancel gesture is sent.
     */
    public void onFlingCancelGesture() {}

    /**
     * Called when a fling has ended.
     */
    public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {}

    /**
     * Called when a fling event was not handled by the renderer.
     */
    public void onUnhandledFlingStartEvent() {}

    /**
     * Called to indicate that a scroll update gesture had been consumed by the page.
     * This callback is called whenever any layer is scrolled (like a frame or div). It is
     * not called when a JS touch handler consumes the event (preventDefault), it is not called
     * for JS-initiated scrolling.
     */
    public void onScrollUpdateGestureConsumed() {}

    /*
     * Called when a scroll gesture has started.
     */
    public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {}

    /*
     * Called when a scroll gesture has stopped.
     */
    public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {}

    /*
     * Called when the scroll offsets or extents may have changed.
     */
    public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {}
}