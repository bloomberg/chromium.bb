// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;

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
        // TODO(twellington): check for custom tabs.
        return !isSideSearchProviderIconAvailable();
    }

    /**
     * @return {@code true} Whether search term refining is available.
     */
    public static boolean isSearchTermRefiningAvailable() {
        // TODO(twellington): check for custom tabs.
        return true;
    }

    /**
     * @return {@code true} Whether the close button is available.
     */
    public static boolean isCloseButtonAvailable() {
        // TODO(twellington): check for custom tabs.
        return false;
    }
}
