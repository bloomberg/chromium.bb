// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.widget.TextView;

/**
 * Handler for actions that happen on suggestion view.
 */
public interface SuggestionViewDelegate {
    /** Triggered when the user selects one of the omnibox suggestions to navigate to. */
    void onSelection();

    /** Triggered when the user selects to refine one of the omnibox suggestions. */
    void onRefineSuggestion();

    /** Triggered when the user long presses the omnibox suggestion. */
    void onLongPress();

    /** Triggered when the user navigates to one of the suggestions without clicking on it. */
    void onSetUrlToSuggestion();

    /** Triggered when the user touches the suggestion view. */
    void onGestureDown();

    /**
     * Triggered when the user touch on the suggestion view finishes.
     * @param timestamp the timestamp for the ACTION_UP event.
     */
    void onGestureUp(long timestamp);

    /**
     * @param line1 The TextView containing the line 1 text whose padding is being calculated.
     * @param maxTextWidth The maximum width the text can occupy.
     * @return any additional padding to be applied to the start of the first line of text.
     */
    int getAdditionalTextLine1StartPadding(TextView line1, int maxTextWidth);
}
