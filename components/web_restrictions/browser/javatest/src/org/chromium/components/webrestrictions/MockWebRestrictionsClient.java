// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webrestrictions;

import org.chromium.base.annotations.CalledByNative;

/**
 * This is a dummy version of the real WebRestrictionsClient, to allow unit testing of the C++
 * code.
 */
public class MockWebRestrictionsClient extends WebRestrictionsClient {
    private String mAuthority;

    @CalledByNative
    static void registerAsMockForTesting() {
        WebRestrictionsClient.registerMockForTesting(new MockWebRestrictionsClient());
    }

    @CalledByNative
    static void unregisterAsMockForTesting() {
        WebRestrictionsClient.registerMockForTesting(null);
    }
    /**
     * Start the web restriction provider and setup the content resolver.
     */
    private MockWebRestrictionsClient() {}

    @Override
    void init(String authority, long nativeProvider) {
        mAuthority = authority;
    }

    /**
     * @return whether the web restriction provider supports requesting access for a blocked url.
     */
    @Override
    boolean supportsRequest() {
        return mAuthority.contains("Good");
    }

    /**
     * Called when the ContentResolverWebRestrictionsProvider is about to be destroyed.
     */
    @Override
    void onDestroy() {}

    /**
     * Whether we can proceed with loading the {@code url}.
     * In case the url is not to be loaded, the web restriction provider can return an optional
     * error page to show instead.
     */
    @Override
    ShouldProceedResult shouldProceed(final String url) {
        return new ShouldProceedResult(mAuthority.contains("Good"), url);
    }

    /**
     * Request permission to load the {@code url}.
     * The web restriction provider returns a {@code boolean} variable indicating whether it was
     * able to forward the request to the appropriate authority who can approve it.
     */
    @Override
    boolean requestPermission(final String url) {
        return mAuthority.contains("Good");
    }
}
