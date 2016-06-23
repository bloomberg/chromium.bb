// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

class TapFarFromPreviousSuppression extends ContextualSearchHeuristic {
    private static final double RETAP_DISTANCE_SQUARED_DP = Math.pow(75, 2);

    private final ContextualSearchTapState mPreviousTapState;
    private final float mPxToDp;
    private final boolean mShouldHandleTap;

    /**
     * Constructs a heuristic to determine if the current Tap should be suppressed because it is
     * far from the previous tap.
     * @param controller The {@link ContextualSearchSelectionController}.
     * @param x The x coordinate of the tap gesture.
     * @param y The y coordinate of the tap gesture.
     */
    TapFarFromPreviousSuppression(ContextualSearchSelectionController controller,
                                  ContextualSearchTapState previousTapState, int x, int y) {
        mPxToDp = controller.getPxToDp();
        mPreviousTapState = previousTapState;
        mShouldHandleTap = shouldHandleTap(x, y);
    }

    @Override
    protected boolean isConditionSatisfied() {
        return !mShouldHandleTap;
    }

    /**
     * @return whether a tap at the given coordinates should be handled or not.
     */
    private boolean shouldHandleTap(int x, int y) {
        if (mPreviousTapState == null) return true;

        return mPreviousTapState.wasSuppressed() || wasTapCloseToPreviousTap(x, y);
    }

    /**
     * Determines whether a tap at the given coordinates is considered "close" to the previous
     * tap.
     */
    private boolean wasTapCloseToPreviousTap(int x, int y) {
        float deltaXDp = (mPreviousTapState.x() - x) * mPxToDp;
        float deltaYDp = (mPreviousTapState.y() - y) * mPxToDp;
        float distanceSquaredDp = deltaXDp * deltaXDp + deltaYDp * deltaYDp;
        return distanceSquaredDp <= RETAP_DISTANCE_SQUARED_DP;
    }
}
