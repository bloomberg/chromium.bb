// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

/**
 * Encapsulates the state of a recent Tap gesture; x, y position and if suppressed.
 */
class ContextualSearchTapState {
    private final boolean mWasSuppressed;
    private final float mX;
    private final float mY;

    /**
     * Constructs a Tap at the given x,y position and indicates if the tap was suppressed or not.
     */
    ContextualSearchTapState(float x, float y, boolean wasSuppressed) {
        mWasSuppressed = wasSuppressed;
        mX = x;
        mY = y;
    }

    /**
     * @return The x coordinate of the Tap.
     */
    float x() {
        return mX;
    }

    /**
     * @return The y coordinate of the Tap.
     */
    float y() {
        return mY;
    }

    /**
     * @return Whether this Tap was suppressed.
     */
    boolean wasSuppressed() {
        return mWasSuppressed;
    }
}
