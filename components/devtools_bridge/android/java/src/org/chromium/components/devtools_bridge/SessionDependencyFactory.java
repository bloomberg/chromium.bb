// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

/**
 * Implements AbstractDataChannel and AbstractPeerConnection on top of org.webrtc.* API.
 * Isolation is needed because some configuration of DevTools bridge may not be based on
 * Java API.
 * In addition abstraction layer isolates SessionBase from complexity of underlying API
 * beside used features.
 */
public abstract class SessionDependencyFactory {
    private interface Constructor {
        SessionDependencyFactory newInstance();
    }

    private static Constructor sConstructor;

    public static SessionDependencyFactory newInstance() {
        return sConstructor.newInstance();
    }

    public static <T extends SessionDependencyFactory> void init(final Class<T> c) {
        sConstructor = new Constructor() {
            @Override
            public SessionDependencyFactory newInstance() {
                try {
                    return c.newInstance();
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        };
    }

    public abstract AbstractPeerConnection createPeerConnection(
            RTCConfiguration config, AbstractPeerConnection.Observer observer);

    public abstract SocketTunnel newSocketTunnelServer(String socketName);

    public abstract void dispose();
}
