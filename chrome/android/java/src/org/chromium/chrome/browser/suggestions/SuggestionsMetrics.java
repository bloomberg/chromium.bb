// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.base.metrics.RecordUserAction;

/**
 * Exposes methods to report suggestions related events, for UMA or Fetch scheduling purposes.
 */
public abstract class SuggestionsMetrics {
    private SuggestionsMetrics() {}

    // UI Element interactions

    public static void recordSurfaceVisible() {
        RecordUserAction.record("Suggestions.SurfaceVisible");
    }

    public static void recordSurfaceHidden() {
        RecordUserAction.record("Suggestions.SurfaceHidden");
    }

    public static void recordTileTapped() {
        RecordUserAction.record("Suggestions.Tile.Tapped");
    }

    public static void recordCardTapped() {
        RecordUserAction.record("Suggestions.Card.Tapped");
    }

    public static void recordCardActionTapped() {
        RecordUserAction.record("Suggestions.Card.ActionTapped");
    }

    public static void recordCardSwipedAway() {
        RecordUserAction.record("Suggestions.Card.SwipedAway");
    }

    // Effect/Purpose of the interactions. Most are recorded in |content_suggestions_metrics.h|

    public static void recordActionViewAll() {
        RecordUserAction.record("Suggestions.Category.ViewAll");
    }
}
