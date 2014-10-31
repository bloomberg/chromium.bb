// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

/**
 * Implementation of ApiaryClientFactory for manual testing.
 */
public class TestApiaryClientFactory extends ApiaryClientFactory {
    @Override
    public String getAPIKey() {
        return "AIzaSyAI1TKbOdqMQ5TltbBT15V5XaIILnDadhI";
    }

    @Override
    public String getOAuthClientId() {
        return "287888336735-v2sniebgl82jgm8lpj6mesv982n505iq.apps.googleusercontent.com";
    }

    @Override
    public String[] getGCMSenderIds() {
        return new String[] { getGCMSenderId() };
    }

    public String getOAuthClientSecret() {
        return "wdMQhGcQqOZmSNteEbqx0IfY";
    }

    public String getGCMSenderId() {
        return "287888336735";
    }
}
