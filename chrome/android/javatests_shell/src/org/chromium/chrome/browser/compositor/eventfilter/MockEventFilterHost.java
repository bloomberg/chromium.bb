// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.eventfilter;

import android.view.MotionEvent;

import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilterHost;

/**
 * Most basic {@link EventFilterHost}.
 */
public class MockEventFilterHost implements EventFilterHost {
    public MockEventFilterHost() {
    }

    @Override
    public boolean propagateEvent(MotionEvent e) {
        return false;
    }

    @Override
    public int getViewportWidth() {
        return 0;
    }
}
