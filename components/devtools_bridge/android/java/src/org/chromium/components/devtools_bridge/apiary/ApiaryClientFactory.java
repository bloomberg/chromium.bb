// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.net.http.AndroidHttpClient;

import org.chromium.base.JNINamespace;

/**
 * Factory for creating clients for external APIs.
 */
@JNINamespace("devtools_bridge::android")
public class ApiaryClientFactory {
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
        return new GCDClient(mHttpClient, nativeGetAPIKey());
    }

    public OAuthClient newOAuthClient() {
        return new OAuthClient(
                mHttpClient, OAUTH_SCOPE, getOAuthClientId(), nativeGetOAuthClientSecret());
    }

    public BlockingGCMRegistrar newGCMRegistrar() {
        return new BlockingGCMRegistrar();
    }

    public void close() {
        mHttpClient.close();
    }

    public String getOAuthClientId() {
        return nativeGetOAuthClientId();
    }

    protected String getAPIKey() {
        return nativeGetAPIKey();
    }

    private native String nativeGetAPIKey();
    private native String nativeGetOAuthClientId();
    private native String nativeGetOAuthClientSecret();
}
