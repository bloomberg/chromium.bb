// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.net.LocalSocketAddress;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Tunnels DevTools UNIX socket over AbstractDataChannel.
 */
public class SocketTunnelServer extends SocketTunnelBase {
    private final LocalSocketAddress mAddress;

    private final ExecutorService mReadingThreadPool = Executors.newCachedThreadPool();

    // Connections with opened client to server stream. If bound to data channel must be accessed
    // on signaling thread.
    private final Map<Integer, Connection> mClientConnections =
            new HashMap<Integer, Connection>();

    // Connections with opened server to client stream. Values are added
    // on signaling thread and removed on reading thread.
    private final ConcurrentMap<Integer, Connection> mServerConnections =
            new ConcurrentHashMap<Integer, Connection>();

    public SocketTunnelServer(String socketName) {
        mAddress = new LocalSocketAddress(socketName);
    }

    @Override
    public AbstractDataChannel unbind() {
        AbstractDataChannel dataChannel = super.unbind();

        mReadingThreadPool.shutdownNow();

        // Safe to access mClientConnections here once AbstractDataChannel.Observer detached.
        for (Connection connection : mClientConnections.values()) {
            connection.terminate();
        }
        mClientConnections.clear();
        return dataChannel;
    }

    public boolean hasConnections() {
        return mClientConnections.size() + mServerConnections.size() > 0;
    }

    @Override
    protected void onReceivedDataPacket(int connectionId, byte[] data) throws ProtocolError {
        checkCalledOnSignalingThread();

        if (!mClientConnections.containsKey(connectionId)) {
            throw new ProtocolError("Unknows conection id");
        }

        mClientConnections.get(connectionId).onReceivedDataPacket(data);
    }

    @Override
    protected void onReceivedControlPacket(int connectionId, byte opCode) throws ProtocolError {
        checkCalledOnSignalingThread();

        switch (opCode) {
            case CLIENT_OPEN:
                onClientOpen(connectionId);
                break;

            case CLIENT_CLOSE:
                onClientClose(connectionId);
                break;

            default:
                throw new ProtocolError("Invalid opCode");
        }
    }

    private void onClientOpen(int connectionId) throws ProtocolError {
        checkCalledOnSignalingThread();

        if (mClientConnections.containsKey(connectionId) ||
                mServerConnections.containsKey(connectionId)) {
            throw new ProtocolError("Conection id already used");
        }

        Connection connection = new Connection(connectionId);
        mClientConnections.put(connectionId, connection);
        mServerConnections.put(connectionId, connection);

        mReadingThreadPool.execute(connection);
    }

    private void onClientClose(int connectionId) throws ProtocolError {
        checkCalledOnSignalingThread();

        if (!mClientConnections.containsKey(connectionId)) {
            throw new ProtocolError("Unknows connection id");
        }

        Connection connection = mClientConnections.get(connectionId);

        connection.closedByClient();
        mClientConnections.remove(connectionId);
    }

    private final class Connection extends ConnectionBase implements Runnable {
        public Connection(int id) {
            super(id);
        }

        public void closedByClient() {
            shutdownOutput();
        }

        @Override
        public void run() {
            assert mServerConnections.containsKey(mId);

            if (connect(mAddress)) {
                sendToDataChannel(buildControlPacket(mId, SERVER_OPEN_ACK));
                runReadingLoop();
            }
            mServerConnections.remove(mId);
            shutdownInput();
            sendToDataChannel(buildControlPacket(mId, SERVER_CLOSE));
        }
    }
}
