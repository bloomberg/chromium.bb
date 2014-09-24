// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.util.Log;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;

/**
 * It allows testing DevTools socket tunneling on a single device.
 *
 * SocketTunnelClient opens LocalServerSocket named |socketToExpose| and
 * tunnels all incoming connections to |socketToReplicate| using
 * SocketTunnelServer and DataPipe between them. All data passes through
 * WebRTC data channel but doens't leave the device.
 */
public class LocalTunnelBridge {
    private static final String TAG = "LocalTunnelBridge";

    private final DataPipe mPipe;
    private final SocketTunnelServer mServer;
    private final SocketTunnelClient mClient;
    private boolean mLogPackets = false;

    private final CountDownLatch mServerDataChannelOpenedFlag = new CountDownLatch(1);
    private final CountDownLatch mServerDataChannelClosedFlag = new CountDownLatch(1);

    public LocalTunnelBridge(String socketToReplicate, String socketToExpose) throws IOException {
        mPipe = new DataPipe();

        mServer = new SocketTunnelServer(socketToReplicate) {
            @Override
            protected void onProtocolError(ProtocolError e) {
                throw new RuntimeException("Protocol error on server", e);
            }

            @Override
            protected void sendToDataChannel(ByteBuffer packet) {
                if (mLogPackets)
                    Log.d(TAG, "Sending " + stringifyServerPacket(packet));
                super.sendToDataChannel(packet);
            }

            @Override
            protected void onReceivedDataPacket(int connectionId, byte[] data)
                    throws ProtocolError {
                if (mLogPackets) {
                    Log.d(TAG, "Received client data packet with " +
                               Integer.toString(data.length) + " bytes");
                }
                super.onReceivedDataPacket(connectionId, data);
            }

            @Override
            protected void onReceivedControlPacket(int connectionId, byte opCode)
                    throws ProtocolError {
                if (mLogPackets) {
                    Log.d(TAG, "Received client control packet");
                }
                super.onReceivedControlPacket(connectionId, opCode);
            }

            @Override
            protected void onSocketException(IOException e, int connectionId) {
                Log.d(TAG, "Server socket exception on " + e +
                           " (connection " + Integer.toString(connectionId) + ")");
                super.onSocketException(e, connectionId);
            }

            protected void onDataChannelOpened() {
                Log.d(TAG, "Server data channel opened");
                super.onDataChannelOpened();
                mServerDataChannelOpenedFlag.countDown();
            }

            protected void onDataChannelClosed() {
                Log.d(TAG, "Client data channel opened");
                super.onDataChannelClosed();
                mServerDataChannelClosedFlag.countDown();
            }
        };

        mServer.bind(mPipe.dataChannel(0));

        mClient = new SocketTunnelClient(socketToExpose) {
            @Override
            protected void onProtocolError(ProtocolError e) {
                throw new RuntimeException("Protocol error on client" + e);
            }

            @Override
            protected void onReceivedDataPacket(int connectionId, byte[] data)
                    throws ProtocolError {
                if (mLogPackets) {
                    Log.d(TAG, "Received server data packet with "
                          + Integer.toString(data.length) + " bytes");
                }
                super.onReceivedDataPacket(connectionId, data);
            }

            @Override
            protected void onReceivedControlPacket(int connectionId, byte opCode)
                    throws ProtocolError {
                if (mLogPackets) {
                    Log.d(TAG, "Received server control packet");
                }
                super.onReceivedControlPacket(connectionId, opCode);
            }

            @Override
            protected void sendToDataChannel(ByteBuffer packet) {
                if (mLogPackets) {
                    Log.d(TAG, "Sending " + stringifyClientPacket(packet));
                }
                super.sendToDataChannel(packet);
            }
        };
        mClient.bind(mPipe.dataChannel(1));
    }

    public void start() {
        mPipe.connect();
    }

    public void stop() {
        mPipe.disconnect();
    }

    public void dispose() {
        mClient.unbind();
        mServer.unbind();
        mPipe.dispose();
    }

    public void waitAllConnectionsClosed() throws InterruptedException {
        while (mServer.hasConnections() || mClient.hasConnections()) {
            Thread.sleep(50);
        }
    }

    private String stringifyDataPacket(String type, PacketDecoder decoder) {
        if (!decoder.isDataPacket()) {
            throw new RuntimeException("Invalid packet");
        }
        return type + "_DATA:" + Integer.toString(decoder.data().length);
    }

    private String stringifyClientPacket(ByteBuffer packet) {
        PacketDecoder decoder = decode(packet);
        if (!decoder.isControlPacket())
            return stringifyDataPacket("CLIENT", decoder);
        switch (decoder.opCode()) {
            case SocketTunnelBase.CLIENT_OPEN:
                return "CLIENT_OPEN " + Integer.valueOf(decoder.connectionId());
            case SocketTunnelBase.CLIENT_CLOSE:
                return "CLIENT_CLOSE " + Integer.valueOf(decoder.connectionId());
            default:
                throw new RuntimeException("Invalid packet");
        }
    }

    private String stringifyServerPacket(ByteBuffer packet) {
        PacketDecoder decoder = decode(packet);
        if (!decoder.isControlPacket())
            return stringifyDataPacket("SERVER", decoder);
        switch (decoder.opCode()) {
            case SocketTunnelBase.SERVER_OPEN_ACK:
                return "SERVER_OPEN_ACK " + Integer.valueOf(decoder.connectionId());
            case SocketTunnelBase.SERVER_CLOSE:
                return "SERVER_CLOSE " + Integer.valueOf(decoder.connectionId());
            default:
                throw new RuntimeException("Invalid packet");
        }
    }

    private PacketDecoder decode(ByteBuffer packet) {
        int position = packet.position();
        packet.position(0);
        if (position == 0) {
            throw new RuntimeException("Empty packet");
        }
        PacketDecoder decoder = PacketDecoder.decode(packet);
        packet.position(position);
        return decoder;
    }
}
