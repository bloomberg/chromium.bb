// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.text.TextUtils;

/**
 * Implements signals for Taps on short and long words.
 * This signal could be used for suppression when the word is short, so we aggregate-log too.
 * We log CTR to UMA for Taps on both short and long words.
 */
class TapWordLengthSuppression extends ContextualSearchHeuristic {
    private static final int MAXIMUM_SHORT_WORD_LENGTH = 3;
    private static final int MAXIMUM_NOT_LONG_WORD_LENGTH = 9;

    private final boolean mIsShortWordSuppressionEnabled;
    private final boolean mIsNotLongWordSuppressionEnabled;
    private final boolean mIsShortWordConditionSatisfied;
    private final boolean mIsNotLongWordConditionSatisfied;

    /**
     * Constructs a heuristic to categorize the Tap based on word length of the tapped word.
     * @param contextualSearchContext The current {@link ContextualSearchContext} so we can inspect
     *        the word tapped.
     */
    TapWordLengthSuppression(ContextualSearchContext contextualSearchContext) {
        mIsShortWordSuppressionEnabled = ContextualSearchFieldTrial.isShortWordSuppressionEnabled();
        mIsNotLongWordSuppressionEnabled =
                ContextualSearchFieldTrial.isNotLongWordSuppressionEnabled();
        mIsShortWordConditionSatisfied =
                !isTapOnWordLongerThan(MAXIMUM_SHORT_WORD_LENGTH, contextualSearchContext);
        mIsNotLongWordConditionSatisfied =
                !isTapOnWordLongerThan(MAXIMUM_NOT_LONG_WORD_LENGTH, contextualSearchContext);
    }

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return mIsShortWordSuppressionEnabled && mIsShortWordConditionSatisfied
                || mIsNotLongWordSuppressionEnabled && mIsNotLongWordConditionSatisfied;
    }

    @Override
    protected void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        if (wasActivatedByTap) {
            ContextualSearchUma.logTapShortWordSeen(
                    wasSearchContentViewSeen, mIsShortWordConditionSatisfied);
            // Log CTR of long words, since not-long word CTR is probably not useful.
            ContextualSearchUma.logTapLongWordSeen(
                    wasSearchContentViewSeen, !mIsNotLongWordConditionSatisfied);
        }
    }

    @Override
    protected boolean shouldAggregateLogForTapSuppression() {
        return true;
    }

    @Override
    protected boolean isConditionSatisfiedForAggregateLogging() {
        // Short-word suppression is a good candidate for aggregate logging of overall suppression.
        return mIsShortWordConditionSatisfied;
    }

    /**
     * @return Whether the tap is on a word longer than the given word length maximum.
     */
    private boolean isTapOnWordLongerThan(
            int maxWordLength, ContextualSearchContext contextualSearchContext) {
        // If setup failed, don't suppress.
        String tappedWord = contextualSearchContext.getTappedWord();
        return !TextUtils.isEmpty(tappedWord) && tappedWord.length() > maxWordLength;
    }
}
