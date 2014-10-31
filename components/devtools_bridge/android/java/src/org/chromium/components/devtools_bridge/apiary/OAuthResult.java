// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

/**
 * Pair of refresh/access tokens fetched with OAuthClient.
 */
public class OAuthResult {
    public String refreshToken;
    public String accessToken;
    public final long expirationTimeMs; // In milliseconds.

    private OAuthResult(String refreshToken, String accessToken, long expirationTimeMs) {
        assert refreshToken != null;

        this.refreshToken = refreshToken;
        this.accessToken = accessToken;
        this.expirationTimeMs = expirationTimeMs;
    }

    public static OAuthResult create(
            String refreshToken, String accessToken, long expirationTimeMs) {
        return refreshToken != null
                ? new OAuthResult(refreshToken, accessToken, expirationTimeMs) : null;
    }
}
