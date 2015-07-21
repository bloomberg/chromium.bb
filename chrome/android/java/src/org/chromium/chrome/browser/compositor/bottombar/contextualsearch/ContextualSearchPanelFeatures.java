// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.util.FeatureUtilities;

/**
 * A utility class meant to determine whether certain features are available in the Search Panel.
 */
public class ContextualSearchPanelFeatures {
    /**
     * Don't instantiate.
     */
    private ContextualSearchPanelFeatures() {}

    /**
     * @return {@code true} Whether the arrow icon is available.
     */
    public static boolean isArrowIconAvailable() {
        return ContextualSearchFieldTrial.isArrowIconEnabled();
    }

    /**
     * @return {@code true} Whether the side search provider icon is available.
     */
    public static boolean isSideSearchProviderIconAvailable() {
        return ContextualSearchFieldTrial.isSideSearchProviderIconEnabled();
    }

    /**
     * @return {@code true} Whether the side search icon is available.
     */
    public static boolean isSearchIconAvailable() {
        return !isSideSearchProviderIconAvailable() && !FeatureUtilities.getCustomTabVisible();
    }

    /**
     * @return {@code true} Whether search term refining is available.
     */
    public static boolean isSearchTermRefiningAvailable() {
        return !FeatureUtilities.getCustomTabVisible();
    }

    /**
     * @return {@code true} Whether the close button is available.
     */
    public static boolean isCloseButtonAvailable() {
        return FeatureUtilities.getCustomTabVisible();
    }

    /**
     * @return {@code true} Whether the close animation should run when the the panel is closed
     *                      due the panel being promoted to a tab.
     */
    public static boolean shouldAnimatePanelCloseOnPromoteToTab() {
        return FeatureUtilities.getCustomTabVisible();
    }
}
