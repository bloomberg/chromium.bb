// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

/**
 * Interface for client or server socket tunnel. Tunnels a socket over a data channel.
 * Client tunnel should be bound to one side and server tunnel to another.
 *
 * Data flow schema looks like this:
 *
 * DevToolsServer
 * <-unix socket->
 * SocketTunnelServer
 * <-data channel->
 * SocketTunnelClient
 * <- unix socket ->
 * Client (DevTools frontend)
 */
interface SocketTunnel {
    void bind(AbstractDataChannel dataChannel);
    AbstractDataChannel unbind();
    boolean isBound();

    void dispose();
}
