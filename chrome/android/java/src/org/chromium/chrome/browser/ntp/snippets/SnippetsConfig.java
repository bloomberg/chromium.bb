// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;

import java.util.Set;

/**
 * Provides configuration details for NTP snippets.
 */
public final class SnippetsConfig {
    /** Map that stores substitution feature flags for tests. */
    private static Set<String> sTestEnabledFeatures;
    private SnippetsConfig() {}

    /**
     * Sets the feature flags to use in JUnit tests, since native calls are not available there.
     * Note: Always set it back to {@code null} at the end of the test!
     */
    @VisibleForTesting
    public static void setTestEnabledFeatures(Set<String> featureList) {
        sTestEnabledFeatures = featureList;
    }

    public static boolean isEnabled() {
        return isFeatureEnabled(ChromeFeatureList.NTP_SNIPPETS);
    }

    public static boolean isSaveToOfflineEnabled() {
        return isFeatureEnabled(ChromeFeatureList.NTP_SNIPPETS_SAVE_TO_OFFLINE)
                && OfflinePageBridge.isBackgroundLoadingEnabled();
    }

    public static boolean isSectionDismissalEnabled() {
        return isFeatureEnabled(ChromeFeatureList.NTP_SUGGESTIONS_SECTION_DISMISSAL);
    }

    private static boolean isFeatureEnabled(String feature) {
        if (sTestEnabledFeatures == null) return ChromeFeatureList.isEnabled(feature);
        return sTestEnabledFeatures.contains(feature);
    }
}
