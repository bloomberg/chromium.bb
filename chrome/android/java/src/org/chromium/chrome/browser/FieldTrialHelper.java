// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.FieldTrialList;

/**
 * Helper to get field trial information.
 */
public class FieldTrialHelper {

    private FieldTrialHelper() {}

    /**
     * This function has been moved to base so it can be used by //sync.
     * Please use {@link FieldTrialList#findFullName()} instead.
     * See http://crbug.com/378035 for context.
     *
     * @param trialName The name of the trial to get the group for.
     * @return The group name chosen for the named trial, or the empty string if the trial does
     *         not exist.
     */
    @Deprecated
    public static String getFieldTrialFullName(String trialName) {
        return FieldTrialList.findFullName(trialName);
    }
}
