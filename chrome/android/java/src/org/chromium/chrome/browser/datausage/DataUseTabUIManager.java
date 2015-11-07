// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datausage;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sessions.SessionTabHelper;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Entry point to manage all UI details for measuring data use within a Tab.
 */
public class DataUseTabUIManager {

    /**
     * Returns true if data use tracking has started within a Tab.
     *
     * @param tab The tab to see if tracking has started in.
     * @return If data use tracking has started.
     */
    public static boolean hasDataUseTrackingStarted(Tab tab) {
        return nativeHasDataUseTrackingStarted(
                SessionTabHelper.sessionIdForTab(tab.getWebContents()), tab.getProfile());
    }

    /**
     * Returns true if data use tracking has ended within a Tab.
     *
     * @param tab The tab to see if tracking has ended in.
     * @return If data use tracking has ended.
     */
    public static boolean hasDataUseTrackingEnded(Tab tab) {
        return nativeHasDataUseTrackingEnded(
                SessionTabHelper.sessionIdForTab(tab.getWebContents()), tab.getProfile());
    }

    private static native boolean nativeHasDataUseTrackingStarted(int tabId, Profile profile);
    private static native boolean nativeHasDataUseTrackingEnded(int tabId, Profile profile);
}
