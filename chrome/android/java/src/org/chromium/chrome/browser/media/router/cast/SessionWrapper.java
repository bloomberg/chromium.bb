// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import com.google.android.gms.cast.Cast;
import com.google.android.gms.common.api.GoogleApiClient;

/**
 * A wrapper around the established Cast application session.
 */
public class SessionWrapper {
    private GoogleApiClient mApiClient;
    private String mSessionId;

    /**
     * Initializes a new {@link SessionWrapper} instance.
     * @param apiClient Google Play Services client used to create the session.
     * @param sessionId the session identifier to use with the Cast SDK.
     */
    public SessionWrapper(GoogleApiClient apiClient, String sessionId) {
        mApiClient = apiClient;
        mSessionId = sessionId;
    }

    /**
     * Stops the Cast application associated with this session.
     */
    public void stop() {
        assert mApiClient != null;

        if (mApiClient.isConnected() || mApiClient.isConnecting()) {
            Cast.CastApi.stopApplication(mApiClient, mSessionId);
        }

        mSessionId = null;
        mApiClient = null;
    }
}
