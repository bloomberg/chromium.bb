// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Represents RTCConfiguration (http://www.w3.org/TR/webrtc/#rtcconfiguration-type).
 * Replacement for List<PeerConnection.IceServer> in Java WebRTC API.
 * Transferable through signaling channel.
 * Immutable.
 */
public class RTCConfiguration {
    /**
     * Single ICE server description.
     */
    public static class IceServer {
        public final String uri;
        public final String username;
        public final String credential;

        public IceServer(String uri, String username, String credential) {
            this.uri = uri;
            this.username = username;
            this.credential = credential;
        }
    }

    public final List<IceServer> iceServers;

    private RTCConfiguration(List<IceServer> iceServers) {
        this.iceServers = Collections.unmodifiableList(new ArrayList<IceServer>(iceServers));
    }

    public RTCConfiguration() {
        this(Collections.<IceServer>emptyList());
    }

    /**
     * Builder for RTCConfiguration.
     */
    public static class Builder {
        private final List<IceServer> mIceServers = new ArrayList<IceServer>();

        public RTCConfiguration build() {
            return new RTCConfiguration(mIceServers);
        }

        public Builder addIceServer(String uri, String username, String credential) {
            mIceServers.add(new IceServer(uri, username, credential));
            return this;
        }

        public Builder addIceServer(String uri) {
            return addIceServer(uri, "", "");
        }
    }
}
