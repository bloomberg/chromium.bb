// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

/**
 * Provides a means for validating whether some condition/criteria has been met.
 * <p>
 * See {@link CriteriaHelper} for usage guidelines.
 */
public abstract class Criteria {

    private String mFailureReason;

    /**
     * Constructs a Criteria with a default failure message.
     */
    public Criteria() {
        this("Criteria not met in allotted time.");
    }

    /**
     * Constructs a Criteria with an explicit message to be shown on failure.
     * @param failureReason The failure reason to be shown.
     */
    public Criteria(String failureReason) {
        mFailureReason = failureReason;
    }

    /**
     * @return Whether the criteria this is testing has been satisfied.
     */
    public abstract boolean isSatisfied();

    /**
     * @return The failure message that will be logged if the criteria is not satisfied within
     *         the specified time range.
     */
    public final String getFailureReason() {
        return mFailureReason;
    }

    /**
     * Updates the message to displayed if this criteria does not succeed in the alloted time.  For
     * correctness, you should be updating this in {@link #isSatisfied()} to ensure the error state
     * is the same that you last checked.
     *
     * @param reason The failure reason to be shown.
     */
    public void updateFailureReason(String reason) {
        mFailureReason = reason;
    }

}
