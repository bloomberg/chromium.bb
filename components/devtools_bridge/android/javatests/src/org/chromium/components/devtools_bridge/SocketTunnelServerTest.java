// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

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

    private DataChannelMock mDataChannelMock;
    private SocketTunnelServer mServer;
    private LocalServerSocket mSocket;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mSocket = new LocalServerSocket(SOCKET_NAME);
    }

    @Override
    public void tearDown() throws Exception {
        mSocket.close();
        if (mServer != null) destroyServer();
        super.tearDown();
    }

    private void createServer() {
        createServer(new DataChannelMock());
    }

    private void createServer(DataChannelMock dataChannel) {
        mDataChannelMock = dataChannel;
        mServer = new SocketTunnelServer(SOCKET_NAME);
        mServer.bind(mDataChannelMock);
    }

    private void destroyServer() {
        mServer.unbind().dispose();
        mServer = null;
    }

    @SmallTest
    public void testOpenDataChannel() {
        createServer();
        mDataChannelMock.open();
    }

    @SmallTest
    public void testDecodeControlPacket() {
        createServer();
        ByteBuffer packet = buildControlPacket(CONNECTION_ID, SocketTunnelBase.SERVER_OPEN_ACK);

        PacketDecoder decoder = PacketDecoder.decode(packet);
        Assert.assertTrue(decoder.isControlPacket());
        Assert.assertEquals(CONNECTION_ID, decoder.connectionId());
        Assert.assertEquals(SocketTunnelBase.SERVER_OPEN_ACK, decoder.opCode());
    }

    @SmallTest
    public void testConnectToSocket() throws IOException {
        createServer();
        LocalSocket socket = connectToSocket(1);
        Assert.assertTrue(mServer.hasConnections());
        socket.close();
    }

    private LocalSocket connectToSocket(int connectionId) throws IOException {
        mDataChannelMock.notifyMessage(
                buildControlPacket(connectionId, SocketTunnelBase.CLIENT_OPEN));
        return mSocket.accept();
    }

    private void sendClose(int connectionId) {
        mDataChannelMock.notifyMessage(
                buildControlPacket(connectionId, SocketTunnelBase.CLIENT_CLOSE));
    }

    private ByteBuffer buildControlPacket(int connectionId, byte opCode) {
        ByteBuffer packet = SocketTunnelBase.buildControlPacket(connectionId, opCode);
        packet.limit(packet.position());
        packet.position(0);
        Assert.assertTrue(packet.remaining() > 0);
        return packet;
    }

    private ByteBuffer buildDataPacket(int connectionId, byte[] data) {
        ByteBuffer packet = SocketTunnelBase.buildDataPacket(connectionId, data, data.length);
        packet.limit(packet.position());
        packet.position(0);
        Assert.assertTrue(packet.remaining() > 0);
        return packet;
    }

    @SmallTest
    public void testReceiveOpenAcknowledgement() throws IOException, InterruptedException {
        createServer();
        LocalSocket socket = connectToSocket(CONNECTION_ID);

        receiveOpenAck(CONNECTION_ID);

        socket.close();
    }

    private PacketDecoder receiveControlPacket(int connectionId) throws InterruptedException {
        PacketDecoder decoder = PacketDecoder.decode(mDataChannelMock.receive());
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

    @SmallTest
    public void testClosingSocket() throws IOException, InterruptedException {
        createServer();
        LocalSocket socket = connectToSocket(CONNECTION_ID);
        receiveOpenAck(CONNECTION_ID);

        socket.shutdownOutput();

        PacketDecoder decoder = PacketDecoder.decode(mDataChannelMock.receive());

        Assert.assertTrue(decoder.isControlPacket());
        Assert.assertEquals(SocketTunnelBase.SERVER_CLOSE, decoder.opCode());
        Assert.assertEquals(CONNECTION_ID, decoder.connectionId());

        socket.close();
    }

    @SmallTest
    public void testReadData() throws IOException, InterruptedException {
        createServer();
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
            PacketDecoder decoder = PacketDecoder.decode(mDataChannelMock.receive());
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

    @SmallTest
    public void testReadLongDataChunk() throws IOException, InterruptedException {
        createServer();
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

    @SmallTest
    public void testReuseConnectionId() throws IOException, InterruptedException {
        createServer();
        LocalSocket socket = connectToSocket(CONNECTION_ID);
        receiveOpenAck(CONNECTION_ID);

        socket.shutdownOutput();
        socket.close();
        receiveClose(CONNECTION_ID);
        sendClose(CONNECTION_ID);

        // Open connection with the same ID
        socket = connectToSocket(CONNECTION_ID);
        receiveOpenAck(CONNECTION_ID);
    }

    private static final byte[] SAMPLE = "Sample".getBytes();

    @SmallTest
    public void testWriteData() throws IOException, InterruptedException {
        createServer();
        LocalSocket socket = connectToSocket(CONNECTION_ID);
        receiveOpenAck(CONNECTION_ID);

        mDataChannelMock.notifyMessage(buildDataPacket(CONNECTION_ID, SAMPLE));

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

    private enum TestStates {
        INITIAL, SENDING, CLOSING, MAY_FINISH_SENDING, SENT, DONE
    }

    @MediumTest
    public void testStopWhileSendingData() throws IOException {

        final TestUtils.StateBarrier<TestStates> barrier =
                new TestUtils.StateBarrier<TestStates>(TestStates.INITIAL);

        createServer(new DataChannelMock() {
            @Override
            public void send(ByteBuffer message, AbstractDataChannel.MessageType type) {
                barrier.advance(TestStates.INITIAL, TestStates.SENDING);
                barrier.advance(TestStates.MAY_FINISH_SENDING, TestStates.SENT);
            }
        });

        LocalSocket socket = connectToSocket(CONNECTION_ID);
        barrier.advance(TestStates.SENDING, TestStates.CLOSING);
        socket.close();

        new Thread() {
            @Override
            public void run() {
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }

                barrier.advance(TestStates.CLOSING, TestStates.MAY_FINISH_SENDING);
            }
        }.start();

        destroyServer();

        barrier.advance(TestStates.SENT, TestStates.DONE);
    }
}
