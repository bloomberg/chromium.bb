// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.nio.ByteBuffer;

/**
 * Decodes data packets of SocketTunnelClient and SocketTunnelServer for tests.
 */
public final class PacketDecoder extends SocketTunnelBase.PacketDecoderBase {
    private boolean mControlPacket = false;
    private boolean mDataPacket;
    private int mOpCode;
    private int mConnectionId;
    private byte[] mData;

    protected void onReceivedDataPacket(int connectionId, byte[] data) {
        mDataPacket = true;
        mConnectionId = connectionId;
        mData = data;
    }

    @Override
    protected void onReceivedControlPacket(int connectionId, byte opCode) {
        mControlPacket = true;
        mOpCode = opCode;
        mConnectionId = connectionId;
    }

    public static PacketDecoder tryDecode(ByteBuffer packet) throws SocketTunnelBase.ProtocolError {
        PacketDecoder decoder = new PacketDecoder();
        decoder.decodePacket(packet);
        return decoder;
    }

    public static PacketDecoder decode(ByteBuffer packet) {
        try {
            return tryDecode(packet);
        } catch (SocketTunnelBase.ProtocolError e) {
            throw new RuntimeException(e);
        }
    }

    public boolean isControlPacket() {
        return mControlPacket;
    }

    public boolean isDataPacket() {
        return mDataPacket;
    }

    public int opCode() {
        assert isControlPacket();
        return mOpCode;
    }

    public int connectionId() {
        return mConnectionId;
    }

    public byte[] data() {
        assert isDataPacket();
        return mData.clone();
    }
}
