// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

/**
 * Base class for client and server that tunnels DevToolsServer's UNIX socket
 * over WebRTC data channel.
 *
 * Server runs on a android device with Chromium (or alike). Client runs where socket
 * needed to be accesses (it could be the same device if socket names are different; this
 * configuration useful for testing).
 *
 * Client listens LocalServerSocket and each time it receives connection it forwards
 * CLIENT_OPEN packet to the server with newly assigned connection id. On receiving this packet
 * server tries to connect to DevToolsServer socket. If succeeded it sends back SERVER_OPEN_ACK
 * with the same connection id. If failed it sends SERVER_CLOSE.
 *
 * When input stream on client shuts down it sends CLIENT_CLOSE. The same with SERVER_CLOSE
 * on the server side (only if SERVER_OPEN_ACK had sent). Between CLIENT_OPEN and CLIENT_CLOSE
 * any amount of data packets may be transferred (the same for SERVER_OPEN_ACK/SERVER_CLOSE
 * on the server side).
 *
 * Since all communication is reliable and ordered it's safe for client to assume that
 * if CLIENT_CLOSE has sent and SERVER_CLOSE has received with the same connection ID this
 * ID is safe to be reused.
 */
public abstract class SocketTunnelBase {
    // Data channel is threadsafe but access to the reference needs synchromization.
    private final ReadWriteLock mDataChanneliReferenceLock = new ReentrantReadWriteLock();
    private volatile AbstractDataChannel mDataChannel;

    // Packet structure encapsulated in buildControlPacket, buildDataPacket and PacketDecoderBase.
    // Structure of control packet:
    // 1-st byte: CONTROL_CONNECTION_ID.
    // 2-d byte: op code.
    // 3-d byte: connection id.
    //
    // Structure of data packet:
    // 1-st byte: connection id.
    // 2..n: data.

    private static final int CONTROL_PACKET_SIZE = 3;

    // Client to server control packets.
    protected static final byte CLIENT_OPEN = (byte) 0;
    protected static final byte CLIENT_CLOSE = (byte) 1;

    // Server to client control packets.
    protected static final byte SERVER_OPEN_ACK = (byte) 0;
    protected static final byte SERVER_CLOSE = (byte) 1;

    // Must not exceed WebRTC limit. Exceeding it closes
    // data channel automatically. TODO(serya): WebRTC limit supposed to be removed.
    static final int READING_BUFFER_SIZE = 4 * 1024;

    private static final int CONTROL_CONNECTION_ID = 0;

    // DevTools supports up to ~10 connections at the time. A few extra IDs usefull for
    // delays in closing acknowledgement.
    protected static final int MIN_CONNECTION_ID = 1;
    protected static final int MAX_CONNECTION_ID = 64;

    // Signaling thread isn't accessible via API. Assumes that first caller
    // checkCalledOnSignalingThread is called on it indeed. It also works well for tests.
    private final AtomicReference<Thread> mSignalingThread = new AtomicReference<Thread>();

    // For writing in socket without blocking signaling thread.
    private final ExecutorService mWritingThread = Executors.newSingleThreadExecutor();

    public boolean isBound() {
        final Lock lock = mDataChanneliReferenceLock.readLock();
        lock.lock();
        try {
            return mDataChannel != null;
        } finally {
            lock.unlock();
        }
    }

    /**
     * Binds the tunnel to the data channel. Tunnel starts its activity when data channel
     * open.
     */
    public void bind(AbstractDataChannel dataChannel) {
        // Observer registrution must not be done in constructor.
        final Lock lock = mDataChanneliReferenceLock.writeLock();
        lock.lock();
        try {
            mDataChannel = dataChannel;
        } finally {
            lock.unlock();
        }
        dataChannel.registerObserver(new DataChannelObserver());
    }

    /**
     * Stops all tunnel activity and returns the prevously bound data channel.
     * It's safe to dispose the data channel after it.
     */
    public AbstractDataChannel unbind() {
        final Lock lock = mDataChanneliReferenceLock.writeLock();
        lock.lock();
        final AbstractDataChannel dataChannel;
        try {
            dataChannel = mDataChannel;
            mDataChannel = null;
        } finally {
            lock.unlock();
        }
        dataChannel.unregisterObserver();
        mSignalingThread.set(null);
        mWritingThread.shutdownNow();
        return dataChannel;
    }

    protected void checkCalledOnSignalingThread() {
        if (!mSignalingThread.compareAndSet(null, Thread.currentThread())) {
            if (mSignalingThread.get() != Thread.currentThread()) {
                throw new RuntimeException("Must be called on signaling thread");
            }
        }
    }

    protected static void checkConnectionId(int connectionId) throws ProtocolError {
        if (connectionId < MIN_CONNECTION_ID || connectionId > MAX_CONNECTION_ID) {
            throw new ProtocolError("Invalid connection id: " + Integer.toString(connectionId));
        }
    }

    protected void onProtocolError(ProtocolError e) {
        checkCalledOnSignalingThread();

        // When integrity of data channel is broken then best way to survive is to close it.
        final Lock lock = mDataChanneliReferenceLock.readLock();
        lock.lock();
        try {
            mDataChannel.close();
        } finally {
            lock.unlock();
        }
    }

    protected abstract void onReceivedDataPacket(int connectionId, byte[] data)
            throws ProtocolError;
    protected abstract void onReceivedControlPacket(int connectionId, byte opCode)
            throws ProtocolError;
    protected void onSocketException(IOException e, int connectionId) {}
    protected void onDataChannelOpened() {}
    protected void onDataChannelClosed() {}

    static ByteBuffer buildControlPacket(int connectionId, byte opCode) {
        ByteBuffer packet = ByteBuffer.allocateDirect(CONTROL_PACKET_SIZE);
        packet.put((byte) CONTROL_CONNECTION_ID);
        packet.put(opCode);
        packet.put((byte) connectionId);
        return packet;
    }

    static ByteBuffer buildDataPacket(int connectionId, byte[] buffer, int count) {
        ByteBuffer packet = ByteBuffer.allocateDirect(count + 1);
        packet.put((byte) connectionId);
        packet.put(buffer, 0, count);
        return packet;
    }

    protected void sendToDataChannel(ByteBuffer packet) {
        packet.limit(packet.position());
        packet.position(0);
        final Lock lock = mDataChanneliReferenceLock.readLock();
        lock.lock();
        try {
            if (mDataChannel != null) {
                mDataChannel.send(packet, AbstractDataChannel.MessageType.BINARY);
            }
        } finally {
            lock.unlock();
        }
    }

    /**
     * Packet decoding exposed for tests.
     */
    abstract static class PacketDecoderBase {
        protected void decodePacket(ByteBuffer packet) throws ProtocolError {
            if (packet.remaining() == 0) {
                throw new ProtocolError("Empty packet");
            }

            int connectionId = packet.get();
            if (connectionId != CONTROL_CONNECTION_ID) {
                checkConnectionId(connectionId);
                byte[] data = new byte[packet.remaining()];
                packet.get(data);
                onReceivedDataPacket(connectionId, data);
            } else {
                if (packet.remaining() != CONTROL_PACKET_SIZE - 1) {
                    throw new ProtocolError("Invalid control packet size");
                }

                byte opCode = packet.get();
                connectionId = packet.get();
                checkConnectionId(connectionId);
                onReceivedControlPacket(connectionId, opCode);
            }
        }

        protected abstract void onReceivedDataPacket(int connectionId, byte[] data)
                throws ProtocolError;
        protected abstract void onReceivedControlPacket(int connectionId, byte opcode)
                throws ProtocolError;
    }

    private final class DataChannelObserver
            extends PacketDecoderBase implements AbstractDataChannel.Observer {
        @Override
        public void onStateChange(AbstractDataChannel.State state) {
            checkCalledOnSignalingThread();

            if (state == AbstractDataChannel.State.OPEN) {
                onDataChannelOpened();
            } else {
                onDataChannelClosed();
            }
        }

        @Override
        public void onMessage(ByteBuffer message) {
            checkCalledOnSignalingThread();

            try {
                decodePacket(message);
            } catch (ProtocolError e) {
                onProtocolError(e);
            }
        }

        @Override
        protected void onReceivedDataPacket(int connectionId, byte[] data) throws ProtocolError {
            checkCalledOnSignalingThread();

            SocketTunnelBase.this.onReceivedDataPacket(connectionId, data);
        }

        @Override
        protected void onReceivedControlPacket(int connectionId, byte opCode) throws ProtocolError {
            checkCalledOnSignalingThread();

            SocketTunnelBase.this.onReceivedControlPacket(connectionId, opCode);
        }
    }

    /**
     * Any problem happened while handling incoming message that breaks state integrity.
     */
    static class ProtocolError extends Exception {
        public ProtocolError(String description) {
            super(description);
        }
    }

    /**
     * Base utility class for client and server connections.
     */
    protected abstract class ConnectionBase {
        protected final int mId;
        protected final LocalSocket mSocket;
        private final AtomicInteger mOpenedStreams = new AtomicInteger(2); // input and output.
        private volatile boolean mConnected;
        private byte[] mBuffer;

        private ConnectionBase(int id, LocalSocket socket, boolean preconnected) {
            mId = id;
            mSocket = socket;
            mConnected = preconnected;
        }

        protected ConnectionBase(int id, LocalSocket socket) {
            this(id, socket, true);
        }

        protected ConnectionBase(int id) {
            this(id, new LocalSocket(), false);
        }

        protected boolean connect(LocalSocketAddress address) {
            assert !mConnected;
            try {
                mSocket.connect(address);
                mConnected = true;
                return true;
            } catch (IOException e) {
                onSocketException(e, mId);
                return false;
            }
        }

        protected void runReadingLoop() {
            mBuffer = new byte[READING_BUFFER_SIZE];
            try {
                boolean open;
                do {
                    open = pump();
                } while (open);
            } catch (IOException e) {
                onSocketException(e, mId);
            } finally {
                mBuffer = null;
            }
        }

        private boolean pump() throws IOException {
            int count = mSocket.getInputStream().read(mBuffer);
            if (count <= 0)
                return false;
            sendToDataChannel(buildDataPacket(mId, mBuffer, count));
            return true;
        }

        protected void writeData(byte[] data) {
            // Called on writing thread.
            try {
                mSocket.getOutputStream().write(data);
            } catch (IOException e) {
                onSocketException(e, mId);
            }
        }

        public void onReceivedDataPacket(final byte[] data) {
            mWritingThread.execute(new Runnable() {
                @Override
                public void run() {
                    writeData(data);
                }
            });
        }

        public void terminate() {
            close();
        }

        protected void shutdownOutput() {
            // Shutdown output on writing thread to make sure all pending writes finished.
            mWritingThread.execute(new Runnable() {
                @Override
                public void run() {
                    shutdownOutputOnWritingThread();
                }
            });
        }

        private void shutdownOutputOnWritingThread() {
            try {
                if (mConnected) mSocket.shutdownOutput();
            } catch (IOException e) {
                onSocketException(e, mId);
            }
            releaseStream();
        }

        protected void shutdownInput() {
            try {
                if (mConnected) mSocket.shutdownInput();
            } catch (IOException e) {
                onSocketException(e, mId);
            }
            releaseStream();
        }

        private void releaseStream() {
            if (mOpenedStreams.decrementAndGet() == 0) close();
        }

        protected void close() {
            try {
                mSocket.close();
            } catch (IOException e) {
                onSocketException(e, mId);
            }
        }
    }
}
