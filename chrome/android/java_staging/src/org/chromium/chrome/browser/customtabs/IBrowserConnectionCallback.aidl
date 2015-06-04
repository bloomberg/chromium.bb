// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.os.Bundle;

/**
 * Interface for the client-provided callback on user navigation.
 */
interface IBrowserConnectionCallback {
    /**
     * To be called when the user triggers an external navigation.
     *
     * @param sessionId As returned by {@link IBrowserConnectionService#newSession}.
     * @param url URL the user has navigated to.
     * @param extras Reserved for future use.
     */
    void onUserNavigation(long sessionId, String url, in Bundle extras);
}
