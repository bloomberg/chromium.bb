// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

/**
 * Information needed for registration in GCD.
 * Instance secret will be bound to oAuthClientId.
 * gcmChannelId will be used for delivering commands.
 * displayName is a human readable name on the client side.
 */
public final class InstanceDescription {
    public final String oAuthClientId;
    public final String gcmChannelId;
    public final String displayName;

    private InstanceDescription(String oAuthClientId, String gcmChannelId, String displayName) {
        assert oAuthClientId != null;
        assert gcmChannelId != null;
        assert displayName != null;

        this.oAuthClientId = oAuthClientId;
        this.gcmChannelId = gcmChannelId;
        this.displayName = displayName;
    }

    /**
     * Builder for InstanceDescription.
     */
    public static final class Builder {
        private String mOAuthClientId;
        private String mGCMChannelId;
        private String mDisplayName;

        public Builder setOAuthClientId(String value) {
            mOAuthClientId = value;
            return this;
        }

        public Builder setGCMChannelId(String value) {
            mGCMChannelId = value;
            return this;
        }

        public Builder setDisplayName(String value) {
            mDisplayName = value;
            return this;
        }

        public InstanceDescription build() {
            return new InstanceDescription(mOAuthClientId, mGCMChannelId, mDisplayName);
        }
    }
}
