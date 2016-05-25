// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

/**
 * Provides configuration details for NTP snippets.
 */
public final class SnippetsConfig {
    private SnippetsConfig() {}

    public static boolean isEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_SNIPPETS)
                && PrefServiceBridge.getInstance().isSearchSuggestEnabled();
    }
}
