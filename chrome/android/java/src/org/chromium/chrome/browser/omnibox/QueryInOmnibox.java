// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.security_state.ConnectionSecurityLevel;

/**
 * Bridge to the native QueryInOmniboxAndroid.
 */
public class QueryInOmnibox {
    /**
     * Extracts query terms from the current URL if it's a SRP URL from the default search engine.
     *
     * @param profile The Profile associated with the tab.
     * @param securityLevel The {@link ConnectionSecurityLevel} of the tab.
     * @param ignoreSecurityLevel When this is set to true, Query in Omnibox ignores the security
     *                            level when determining whether to display search terms or not.
     *                            Use this to avoid a flicker during page load for an SRP URL
     *                            before the SSL state updates.
     * @param url The URL to extract search terms from.
     * @return The extracted search terms. Returns null if the Omnibox should not display the
     *         search terms.
     */
    public static String getDisplaySearchTerms(Profile profile,
            @ConnectionSecurityLevel int securityLevel, boolean ignoreSecurityLevel, String url) {
        return nativeGetDisplaySearchTerms(profile, securityLevel, ignoreSecurityLevel, url);
    }

    private static native String nativeGetDisplaySearchTerms(
            Profile profile, int securityLevel, boolean ignoreSecurityLevel, String url);
}
