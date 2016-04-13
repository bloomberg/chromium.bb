// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import java.util.HashSet;
import java.util.Set;

/**
 * A set of {@link ContextualSearchHeuristic}s that support experimentation and logging.
 */
public class TapSuppressionHeuristics {
    private final Set<ContextualSearchHeuristic> mTapHeuristics;

    /**
     * Gets all the heuristics needed for Tap suppression.
     * @param selectionController The {@link ContextualSearchSelectionController}
     * @param x The x position of the Tap.
     * @param y The y position of the Tap.
     */
    TapSuppressionHeuristics(
            ContextualSearchSelectionController selectionController, int x, int y) {
        mTapHeuristics = new HashSet<ContextualSearchHeuristic>();
        RecentScrollTapSuppression scrollTapExperiment =
                new RecentScrollTapSuppression(selectionController);
        mTapHeuristics.add(scrollTapExperiment);
        TapFarFromPreviousSuppression farFromPreviousHeuristic =
                new TapFarFromPreviousSuppression(selectionController, x, y);
        mTapHeuristics.add(farFromPreviousHeuristic);
    }

    /**
     * Logs the results seen for the heuristics and whether they would have had their condition
     * satisfied if enabled.
     * @param wasSearchContentViewSeen Whether the panel contents were seen.
     * @param wasActivatedByTap Whether the panel was activated by a Tap or not.
     */
    public void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        for (ContextualSearchHeuristic heuristic : mTapHeuristics) {
            heuristic.logResultsSeen(wasSearchContentViewSeen, wasActivatedByTap);
        }
    }

    /**
     * Logs the condition state for all the Tap suppression heuristics.
     */
    void logConditionState() {
        // TODO(donnd): consider doing this logging automatically in the constructor rather than
        // leaving this an optional separate method.
        for (ContextualSearchHeuristic heuristic : mTapHeuristics) {
            heuristic.logConditionState();
        }
    }

    /**
     * @return Whether the Tap should be suppressed (due to a heuristic condition being satisfied).
     */
    boolean shouldSuppressTap() {
        for (ContextualSearchHeuristic heuristic : mTapHeuristics) {
            if (heuristic.isConditionSatisfied()) return true;
        }
        return false;
    }
}
