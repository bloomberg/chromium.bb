// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.eventfilter;

import android.view.MotionEvent;
import android.view.View;

/**
 * The interface an host view that want its event to be filtered need to provide.
 */
public interface EventFilterHost {

    /**
     * Propagates the event the event filter is not consuming.
     *
     * @param e The {@link MotionEvent} the filter is forwarding.
     * @return Whether the propagated event has been handled.
     */
    public boolean propagateEvent(MotionEvent e);

    /**
     * Notifies when a gesture starts. A gesture may live across
     * {@link com.google.android.apps.chrome.tabs.layout.Layout} changes.
     */
    public void onStartGesture();

    /**
     * Notifies when a gesture ends.
     */
    public void onEndGesture();

    /**
     * @return The {@link View} capturing the event in the first place.
     */
    public View getView();
}