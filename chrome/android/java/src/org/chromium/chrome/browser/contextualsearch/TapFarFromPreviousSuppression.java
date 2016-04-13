// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

class TapFarFromPreviousSuppression extends ContextualSearchHeuristic {
    private static final double RETAP_DISTANCE_SQUARED_DP = Math.pow(75, 2);

    private final boolean mWasLastTapValid;
    private final float mX;
    private final float mY;
    private final float mPxToDp;
    private final boolean mShouldHandleTap;

    TapFarFromPreviousSuppression(ContextualSearchSelectionController controller, int x, int y) {
        mPxToDp = controller.getPxToDp();
        mX = controller.getLastX();
        mY = controller.getLastY();
        mWasLastTapValid = controller.getWasLastTapValid();
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
        return !mWasLastTapValid || wasTapCloseToPreviousTap(x, y);
    }

    /**
     * Determines whether a tap at the given coordinates is considered "close" to the previous
     * tap.
     */
    private boolean wasTapCloseToPreviousTap(int x, int y) {
        float deltaXDp = (mX - x) * mPxToDp;
        float deltaYDp = (mY - y) * mPxToDp;
        float distanceSquaredDp = deltaXDp * deltaXDp + deltaYDp * deltaYDp;
        return distanceSquaredDp <= RETAP_DISTANCE_SQUARED_DP;
    }
}
