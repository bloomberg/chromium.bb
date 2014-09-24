// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.util.Log;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Listens LocalServerSocket and tunnels all connections to the SocketTunnelServer.
 */
public class SocketTunnelClient extends SocketTunnelBase {
    private static final String TAG = "SocketTunnelClient";

    private enum State {
        INITIAL, RUNNING, STOPPED
    }

    private final AtomicReference<State> mState = new AtomicReference<State>(State.INITIAL);

    private final LocalServerSocket mSocket;
    private final ExecutorService mThreadPool = Executors.newCachedThreadPool();

    // Connections with opened server to client stream. Always accesses on signaling thread.
    private final Map<Integer, Connection> mServerConnections =
            new HashMap<Integer, Connection>();

    // Accepted connections are kept here until server returns SERVER_OPEN_ACK or SERVER_CLOSE.
    // New connections are added in the listening loop, checked and removed on signaling thread.
    // So add/read/remove synchronized through message round trip.
    private final ConcurrentMap<Integer, Connection> mPendingConnections =
            new ConcurrentHashMap<Integer, Connection>();

    private final IdRegistry mIdRegistry = new IdRegistry(MIN_CONNECTION_ID, MAX_CONNECTION_ID, 2);

    /**
     * This class responsible for generating valid connection IDs. It count usage of connection:
     * one user for client to server stream and one for server to client one. When both are closed
     * it's safe to reuse ID.
     */
    private static final class IdRegistry {
        private final int[] mLocks;
        private final int mMin;
        private final int mMax;
        private final int mMaxLocks;
        private final Object mLock = new Object();

        public IdRegistry(int minId, int maxId, int maxLocks) {
            assert minId < maxId;
            assert maxLocks > 0;

            mMin = minId;
            mMax = maxId;
            mMaxLocks = maxLocks;
            mLocks = new int[maxId - minId + 1];
        }

        public void lock(int id) {
            synchronized (mLock) {
                int index = toIndex(id);
                if (mLocks[index] == 0 || mLocks[index] == mMaxLocks) {
                    throw new RuntimeException();
                }
                mLocks[index]++;
            }
        }

        public void release(int id) {
            synchronized (mLock) {
                int index = toIndex(id);
                if (mLocks[index] == 0) {
                    throw new RuntimeException("Releasing unlocked id " + Integer.toString(id));
                }
                mLocks[index]--;
            }
        }

        public boolean isLocked(int id) {
            synchronized (mLock) {
                return mLocks[toIndex(id)] > 0;
            }
        }

        public int generate() throws NoIdAvailableException {
            synchronized (mLock) {
                for (int id = mMin; id != mMax; id++) {
                    int index = toIndex(id);
                    if (mLocks[index] == 0) {
                        mLocks[index] = 1;
                        return id;
                    }
                }
            }
            throw new NoIdAvailableException();
        }

        private int toIndex(int id) {
            if (id < mMin || id > mMax) {
                throw new RuntimeException();
            }
            return id - mMin;
        }
    }

    private static class NoIdAvailableException extends Exception {}

    public SocketTunnelClient(String socketName) throws IOException {
        mSocket = new LocalServerSocket(socketName);
    }

    public boolean hasConnections() {
        return mServerConnections.size() + mPendingConnections.size() > 0;
    }

    @Override
    public AbstractDataChannel unbind() {
        AbstractDataChannel dataChannel = super.unbind();
        close();
        return dataChannel;
    }

    public void close() {
        if (mState.get() != State.STOPPED) closeSocket();
    }

    @Override
    protected void onReceivedDataPacket(int connectionId, byte[] data) throws ProtocolError {
        checkCalledOnSignalingThread();

        if (!mServerConnections.containsKey(connectionId))
            throw new ProtocolError("Unknows connection id");

        mServerConnections.get(connectionId).onReceivedDataPacket(data);
    }

    @Override
    protected void onReceivedControlPacket(int connectionId, byte opCode) throws ProtocolError {
        switch (opCode) {
            case SERVER_OPEN_ACK:
                onServerOpenAck(connectionId);
                break;

            case SERVER_CLOSE:
                onServerClose(connectionId);
                break;

            default:
                throw new ProtocolError("Invalid opCode");
        }
    }

    private void onServerOpenAck(int connectionId) throws ProtocolError {
        checkCalledOnSignalingThread();

        if (mServerConnections.containsKey(connectionId)) {
            throw new ProtocolError("Connection already acknowledged");
        }

        if (!mPendingConnections.containsKey(connectionId)) {
            throw new ProtocolError("Unknow connection id");
        }

        // Check/get is safe since it can be only removed on this thread.
        Connection connection = mPendingConnections.get(connectionId);
        mPendingConnections.remove(connectionId);

        mServerConnections.put(connectionId, connection);

        // Lock for client to server stream.
        mIdRegistry.lock(connectionId);
        mThreadPool.execute(connection);
    }

    private void onServerClose(int connectionId) throws ProtocolError {
        checkCalledOnSignalingThread();

        if (mServerConnections.containsKey(connectionId)) {
            Connection connection = mServerConnections.get(connectionId);
            mServerConnections.remove(connectionId);
            mIdRegistry.release(connectionId); // Release sever to client stream.
            connection.closedByServer();
        } else if (mPendingConnections.containsKey(connectionId)) {
            Connection connection = mPendingConnections.get(connectionId);
            mPendingConnections.remove(connectionId);
            connection.closedByServer();
            sendToDataChannel(buildControlPacket(connectionId, CLIENT_CLOSE));
            mIdRegistry.release(connectionId); // Release sever to client stream.
        } else {
            throw new ProtocolError("Closing unknown connection");
        }
    }

    @Override
    protected void onDataChannelOpened() {
        if (!mState.compareAndSet(State.INITIAL, State.RUNNING)) {
            throw new InvalidStateException();
        }

        mThreadPool.execute(new Runnable() {
            @Override
            public void run() {
                runListenLoop();
            }
        });
    }

    @Override
    protected void onDataChannelClosed() {
        // All new connections will be rejected.
        if (!mState.compareAndSet(State.RUNNING, State.STOPPED)) {
            throw new InvalidStateException();
        }

        for (Connection connection : mServerConnections.values()) {
            connection.terminate();
        }

        for (Connection connection : mPendingConnections.values()) {
            connection.terminate();
        }

        closeSocket();

        mThreadPool.shutdown();
    }

    private void closeSocket() {
        try {
            mSocket.close();
        } catch (IOException e) {
            Log.d(TAG, "Failed to close socket: " + e);
            onSocketException(e, -1);
        }
    }

    private void runListenLoop() {
        try {
            while (true) {
                LocalSocket socket = mSocket.accept();
                State state = mState.get();
                if (mState.get() == State.RUNNING) {
                    // Make sure no socket processed when stopped.
                    clientOpenConnection(socket);
                } else {
                    socket.close();
                }
            }
        } catch (IOException e) {
            if (mState.get() != State.RUNNING) {
                onSocketException(e, -1);
            }
            // Else exception expected (socket closed).
        }
    }

    private void clientOpenConnection(LocalSocket socket) throws IOException {
        try {
            int id = mIdRegistry.generate();  // id generated locked for server to client stream.
            Connection connection = new Connection(id, socket);
            mPendingConnections.put(id, connection);
            sendToDataChannel(buildControlPacket(id, CLIENT_OPEN));
        } catch (NoIdAvailableException e) {
            socket.close();
        }
    }

    private final class Connection extends ConnectionBase implements Runnable {
        public Connection(int id, LocalSocket socket) {
            super(id, socket);
        }

        public void closedByServer() {
            shutdownOutput();
        }

        @Override
        public void run() {
            assert mIdRegistry.isLocked(mId);

            runReadingLoop();

            shutdownInput();
            sendToDataChannel(buildControlPacket(mId, CLIENT_CLOSE));
            mIdRegistry.release(mId);  // Unlock for client to server stream.
        }
    }

    /**
     * Method called in inappropriate state.
     */
    public static class InvalidStateException extends RuntimeException {}
}
