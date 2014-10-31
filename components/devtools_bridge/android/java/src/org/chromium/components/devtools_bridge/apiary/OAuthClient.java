// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.util.JsonReader;

import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;

/**
 * Google authentication client. Fetches a pair of refresh/access tokens for a
 * secret received while registering the instance in GCD.
 */
public class OAuthClient {
    public static final String API_BASE = "https://accounts.google.com/o/oauth2";
    public static final String ENCODING = "UTF-8";
    public static final String CONTENT_TYPE = "application/x-www-form-urlencoded";

    private final HttpClient mHttpClient;
    private final String mScope;
    private final String mClientId;
    private final String mClientSecret;

    OAuthClient(HttpClient httpClient, String scope, String clientId, String clientSecret) {
        assert httpClient != null;
        assert scope != null;
        assert clientId != null;
        assert clientSecret != null;

        mHttpClient = httpClient;
        mScope = scope;
        mClientId = clientId;
        mClientSecret = clientSecret;
    }

    public OAuthResult authenticate(String secret) throws IOException {
        final long startTimeMs = System.currentTimeMillis();

        String content =
                "client_id=" + urlEncode(mClientId)
                + "&client_secret=" + urlEncode(mClientSecret)
                + "&scope=" + urlEncode(mScope)
                + "&code=" + urlEncode(secret)
                + "&redirect_uri=oob"
                + "&grant_type=authorization_code";

        return mHttpClient.execute(
                newHttpPost("/token", content),
                new JsonResponseHandler<OAuthResult>() {
                    @Override
                    public OAuthResult readResponse(JsonReader reader) throws IOException {
                        return readResponse(reader, startTimeMs);
                    }
                });
    }

    private HttpPost newHttpPost(String path, String content) throws UnsupportedEncodingException {
        HttpPost request = new HttpPost(API_BASE + "/token");
        request.setEntity(new StringEntity(content, ENCODING));
        request.addHeader("Content-Type", CONTENT_TYPE);
        return request;
    }

    private static String urlEncode(String value) {
        try {
            return URLEncoder.encode(value, ENCODING);
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException(e);
        }
    }

    static OAuthResult readResponse(JsonReader reader, long startTimeMs) throws IOException {
        String refreshToken = null;
        String accessToken = null;
        long expiresInS = 0; // In seconds.

        reader.beginObject();
        while (reader.hasNext()) {
            String name = reader.nextName();
            if (name.equals("refresh_token")) {
                refreshToken = reader.nextString();
            } else if (name.equals("access_token")) {
                accessToken = reader.nextString();
            } else if (name.equals("expires_in")) {
                expiresInS = reader.nextLong();
            } else {
                reader.skipValue();
            }
        }
        reader.endObject();

        return OAuthResult.create(
                refreshToken, accessToken, startTimeMs + expiresInS * 1000);
    }
}
