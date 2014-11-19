// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;

import junit.framework.Assert;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;

/**
 * Tests for {@link SocketTunnelServer}
 */
public class SocketTunnelServerTest extends InstrumentationTestCase {
    private static final int CONNECTION_ID = 30;
    private static final String SOCKET_NAME = "ksdjhflksjhdflk";

    private static final int SERVER_CHANNEL = 1;
    private static final int CLIENT_CHANNEL = 0;

    private SessionDependencyFactory mFactory;
    private DataPipe mPipe;
    private SocketTunnel mServer;
    private LocalServerSocket mSocket;
    private DataChannelObserverMock mObserverMock;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mFactory = SessionDependencyFactory.newInstance();
        mPipe = new DataPipe(mFactory);
        mServer = mFactory.newSocketTunnelServer(SOCKET_NAME);
        mServer.bind(mPipe.dataChannel(SERVER_CHANNEL));
        mSocket = new LocalServerSocket(SOCKET_NAME);
        mObserverMock = new DataChannelObserverMock();
        mPipe.dataChannel(CLIENT_CHANNEL).registerObserver(mObserverMock);
        mPipe.negotiate();
        mPipe.awaitConnected();
    }

    @Override
    public void tearDown() throws Exception {
        mPipe.dataChannel(CLIENT_CHANNEL).unregisterObserver();
        mServer.unbind();
        mPipe.dispose();
        mFactory.dispose();
        super.tearDown();
    }

    private void sendPacket(ByteBuffer packet) {
        packet.position(0);
        mPipe.send(CLIENT_CHANNEL, packet);
    }

    @MediumTest
    public void testConnectToSocket() throws IOException {
        LocalSocket socket = connectToSocket(1);
        socket.close();
    }

    private LocalSocket connectToSocket(int connectionId) throws IOException {
        sendPacket(SocketTunnelBase.buildControlPacket(
                connectionId, SocketTunnelBase.CLIENT_OPEN));
        return mSocket.accept();
    }

    private void sendClose(int connectionId) {
        sendPacket(SocketTunnelBase.buildControlPacket(
                connectionId, SocketTunnelBase.CLIENT_CLOSE));
    }

    @MediumTest
    public void testReceiveOpenAcknowledgement() throws IOException, InterruptedException {
        LocalSocket socket = connectToSocket(CONNECTION_ID);

        receiveOpenAck(CONNECTION_ID);

        socket.close();
    }

    private PacketDecoder receivePacket() throws InterruptedException {
        byte[] bytes = mObserverMock.received.take();
        ByteBuffer buffer = ByteBuffer.allocate(bytes.length);
        buffer.put(bytes);
        buffer.position(0);
        return PacketDecoder.decode(buffer);
    }

    private PacketDecoder receiveControlPacket(int connectionId) throws InterruptedException {
        PacketDecoder decoder = receivePacket();
        Assert.assertTrue(decoder.isControlPacket());
        Assert.assertEquals(connectionId, decoder.connectionId());
        return decoder;
    }

    private void receiveOpenAck(int connectionId) throws InterruptedException {
        PacketDecoder decoder = receiveControlPacket(connectionId);
        Assert.assertEquals(SocketTunnelBase.SERVER_OPEN_ACK, decoder.opCode());
    }

    private void receiveClose(int connectionId) throws InterruptedException {
        PacketDecoder decoder = receiveControlPacket(connectionId);
        Assert.assertEquals(SocketTunnelBase.SERVER_CLOSE, decoder.opCode());
    }

    @MediumTest
    public void testClosingSocket() throws IOException, InterruptedException {
        LocalSocket socket = connectToSocket(CONNECTION_ID);
        receiveOpenAck(CONNECTION_ID);

        socket.close();

        PacketDecoder decoder = receiveControlPacket(CONNECTION_ID);

        Assert.assertEquals(SocketTunnelBase.SERVER_CLOSE, decoder.opCode());
    }

    @MediumTest
    public void testReadData() throws IOException, InterruptedException {
        LocalSocket socket = connectToSocket(CONNECTION_ID);
        receiveOpenAck(CONNECTION_ID);

        byte[] sample = "Sample".getBytes();

        socket.getOutputStream().write(sample);
        socket.getOutputStream().flush();
        socket.shutdownOutput();

        ByteBuffer result = receiveData(CONNECTION_ID, sample.length);
        Assert.assertEquals(ByteBuffer.wrap(sample), result);
    }

    private ByteBuffer receiveData(int connectionId, int length) throws InterruptedException {
        ByteBuffer result = ByteBuffer.allocate(length);

        while (true) {
            PacketDecoder decoder = receivePacket();
            if (decoder.isDataPacket()) {
                Assert.assertEquals(connectionId, decoder.connectionId());
                result.put(decoder.data());
            } else if (decoder.isControlPacket()) {
                Assert.assertEquals(SocketTunnelBase.SERVER_CLOSE, decoder.opCode());
                Assert.assertEquals(connectionId, decoder.connectionId());
                break;
            }
        }
        result.limit(result.position());
        result.position(0);
        return result;
    }

    private int sum(int[] values) {
        int result = 0;
        for (int v : values)
            result += v;
        return result;
    }

    private static final int[] CHUNK_SIZES =
            new int[] { 0, 1, 5, 100, 1000, SocketTunnelBase.READING_BUFFER_SIZE * 2 };

    @MediumTest
    public void testReadLongDataChunk() throws IOException, InterruptedException {
        LocalSocket socket = connectToSocket(CONNECTION_ID);
        receiveOpenAck(CONNECTION_ID);

        byte[] buffer = new byte[CHUNK_SIZES[CHUNK_SIZES.length - 1]];
        ByteBuffer sentData = ByteBuffer.allocate(sum(CHUNK_SIZES));
        OutputStream stream = socket.getOutputStream();
        byte next = 0;
        int prevSize = 0;
        for (int size : CHUNK_SIZES) {
            while (prevSize < size)
                buffer[prevSize++] = next++;

            stream.write(buffer, 0, size);
            sentData.put(buffer, 0, size);
        }

        socket.shutdownOutput();

        sentData.limit(sentData.position());
        sentData.position(0);
        ByteBuffer readData = receiveData(CONNECTION_ID, sentData.limit());

        Assert.assertEquals(sentData, readData);
    }

    @MediumTest
    public void testReuseConnectionId() throws IOException, InterruptedException {
        LocalSocket socket = connectToSocket(CONNECTION_ID);
        receiveOpenAck(CONNECTION_ID);

        socket.close();
        receiveClose(CONNECTION_ID);
        sendClose(CONNECTION_ID);

        // Open connection with the same ID
        socket = connectToSocket(CONNECTION_ID);
        receiveOpenAck(CONNECTION_ID);
    }

    private static final byte[] SAMPLE = "Sample".getBytes();

    @MediumTest
    public void testWriteData() throws IOException, InterruptedException {
        LocalSocket socket = connectToSocket(CONNECTION_ID);
        receiveOpenAck(CONNECTION_ID);

        sendPacket(SocketTunnelBase.buildDataPacket(CONNECTION_ID, SAMPLE, SAMPLE.length));

        byte[] result = new byte[SAMPLE.length];
        int read = 0;
        while (read < SAMPLE.length) {
            int count = socket.getInputStream().read(result, 0, SAMPLE.length - read);
            Assert.assertTrue(count > 0);
            read += count;
        }

        Assert.assertEquals(ByteBuffer.wrap(SAMPLE), ByteBuffer.wrap(result));

        socket.close();
    }
}
