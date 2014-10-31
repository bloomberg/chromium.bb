// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.net.http.AndroidHttpClient;

/**
 * Factory for creating clients for external APIs.
 */
public abstract class ApiaryClientFactory {
    private static final String USER_AGENT = "DevTools bridge";

    public static final String OAUTH_SCOPE = "https://www.googleapis.com/auth/clouddevices";

    protected final AndroidHttpClient mHttpClient = AndroidHttpClient.newInstance(USER_AGENT);

    /**
     * Creates a new GCD client with auth token.
     */
    public GCDClient newGCDClient(String oAuthToken) {
        return new GCDClient(mHttpClient, getAPIKey(), oAuthToken);
    }

    /**
     * Creates a new anonymous client. GCD requires client been not authenticated by user or
     * device credentials for finalizing registration.
     */
    public GCDClient newAnonymousGCDClient() {
        return new GCDClient(mHttpClient, getAPIKey());
    }

    public OAuthClient newOAuthClient() {
        return new OAuthClient(
                mHttpClient, OAUTH_SCOPE, getOAuthClientId(), getOAuthClientSecret());
    }

    public BlockingGCMRegistrar newGCMRegistrar() {
        return new BlockingGCMRegistrar() {
                    @Override
                    protected String[] getSenderIds() {
                        return getGCMSenderIds();
                    }
                };
    }

    public void close() {
        mHttpClient.close();
    }

    public abstract String getAPIKey();
    public abstract String getOAuthClientId();
    public abstract String getOAuthClientSecret();
    public abstract String[] getGCMSenderIds();
}
