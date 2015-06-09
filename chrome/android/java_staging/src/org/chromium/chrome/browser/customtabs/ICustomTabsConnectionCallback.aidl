// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.os.Bundle;

/**
 * Interface for the client-provided callback on user navigation.
 */
interface ICustomTabsConnectionCallback {
    /**
     * To be called when a page navigation starts.
     *
     * @param sessionId As returned by {@link ICustomTabsConnectionService#newSession}.
     * @param url URL the user has navigated to.
     * @param extras Reserved for future use.
     */
    oneway void onUserNavigationStarted(long sessionId, String url, in Bundle extras);

    /**
     * To be called when a page navigation finishes.
     *
     * @param sessionId As returned by {@link ICustomTabsConnectionService#newSession}.
     * @param url URL the user has navigated to.
     * @param extras Reserved for future use.
     */
    oneway void onUserNavigationFinished(long sessionId, String url, in Bundle extras);
}
