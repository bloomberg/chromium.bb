// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system.impl;

import android.support.test.filters.SmallTest;

import org.chromium.mojo.MojoTestCase;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.Core.HandleSignals;
import org.chromium.mojo.system.DataPipe;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.InvalidHandle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.ResultAnd;
import org.chromium.mojo.system.SharedBufferHandle;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;

/**
 * Testing the core API.
 */
public class CoreImplTest extends MojoTestCase {
    private static final long RUN_LOOP_TIMEOUT_MS = 5;

    private static final ScheduledExecutorService WORKER =
            Executors.newSingleThreadScheduledExecutor();

    private static final HandleSignals ALL_SIGNALS =
            HandleSignals.none().setPeerClosed(true).setReadable(true).setWritable(true);

    private List<Handle> mHandlesToClose = new ArrayList<Handle>();

    /**
     * @see MojoTestCase#tearDown()
     */
    @Override
    protected void tearDown() throws Exception {
        MojoException toThrow = null;
        for (Handle handle : mHandlesToClose) {
            try {
                handle.close();
            } catch (MojoException e) {
                if (toThrow == null) {
                    toThrow = e;
                }
            }
        }
        if (toThrow != null) {
            throw toThrow;
        }
        super.tearDown();
    }

    private void addHandleToClose(Handle handle) {
        mHandlesToClose.add(handle);
    }

    private void addHandlePairToClose(Pair<? extends Handle, ? extends Handle> handles) {
        mHandlesToClose.add(handles.first);
        mHandlesToClose.add(handles.second);
    }

    private static void checkSendingMessage(MessagePipeHandle in, MessagePipeHandle out) {
        Random random = new Random();

        // Writing a random 8 bytes message.
        byte[] bytes = new byte[8];
        random.nextBytes(bytes);
        ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
        buffer.put(bytes);
        in.writeMessage(buffer, null, MessagePipeHandle.WriteFlags.NONE);

        // Read the message back.
        ResultAnd<MessagePipeHandle.ReadMessageResult> result =
                out.readMessage(MessagePipeHandle.ReadFlags.NONE);
        assertEquals(MojoResult.OK, result.getMojoResult());
        assertTrue(Arrays.equals(bytes, result.getValue().mData));
        assertEquals(0, result.getValue().mHandles.size());
    }

    private static void checkSendingData(DataPipe.ProducerHandle in, DataPipe.ConsumerHandle out) {
        Random random = new Random();

        // Writing a random 8 bytes message.
        byte[] bytes = new byte[8];
        random.nextBytes(bytes);
        ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
        buffer.put(bytes);
        ResultAnd<Integer> result = in.writeData(buffer, DataPipe.WriteFlags.NONE);
        assertEquals(MojoResult.OK, result.getMojoResult());
        assertEquals(bytes.length, result.getValue().intValue());

        // Query number of bytes available.
        ResultAnd<Integer> readResult = out.readData(null, DataPipe.ReadFlags.none().query(true));
        assertEquals(MojoResult.OK, readResult.getMojoResult());
        assertEquals(bytes.length, readResult.getValue().intValue());

        // Peek data into a buffer.
        ByteBuffer peekBuffer = ByteBuffer.allocateDirect(bytes.length);
        readResult = out.readData(peekBuffer, DataPipe.ReadFlags.none().peek(true));
        assertEquals(MojoResult.OK, readResult.getMojoResult());
        assertEquals(bytes.length, readResult.getValue().intValue());
        assertEquals(bytes.length, peekBuffer.limit());
        byte[] peekBytes = new byte[bytes.length];
        peekBuffer.get(peekBytes);
        assertTrue(Arrays.equals(bytes, peekBytes));

        // Read into a buffer.
        ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(bytes.length);
        readResult = out.readData(receiveBuffer, DataPipe.ReadFlags.NONE);
        assertEquals(MojoResult.OK, readResult.getMojoResult());
        assertEquals(bytes.length, readResult.getValue().intValue());
        assertEquals(0, receiveBuffer.position());
        assertEquals(bytes.length, receiveBuffer.limit());
        byte[] receivedBytes = new byte[bytes.length];
        receiveBuffer.get(receivedBytes);
        assertTrue(Arrays.equals(bytes, receivedBytes));
    }

    private static void checkSharing(SharedBufferHandle in, SharedBufferHandle out) {
        Random random = new Random();

        ByteBuffer buffer1 = in.map(0, 8, SharedBufferHandle.MapFlags.NONE);
        assertEquals(8, buffer1.capacity());
        ByteBuffer buffer2 = out.map(0, 8, SharedBufferHandle.MapFlags.NONE);
        assertEquals(8, buffer2.capacity());

        byte[] bytes = new byte[8];
        random.nextBytes(bytes);
        buffer1.put(bytes);

        byte[] receivedBytes = new byte[bytes.length];
        buffer2.get(receivedBytes);

        assertTrue(Arrays.equals(bytes, receivedBytes));

        in.unmap(buffer1);
        out.unmap(buffer2);
    }

    /**
     * Testing that Core can be retrieved from a handle.
     */
    @SmallTest
    public void testGetCore() {
        Core core = CoreImpl.getInstance();

        Pair<? extends Handle, ? extends Handle> handles = core.createMessagePipe(null);
        addHandlePairToClose(handles);
        assertEquals(core, handles.first.getCore());
        assertEquals(core, handles.second.getCore());

        handles = core.createDataPipe(null);
        addHandlePairToClose(handles);
        assertEquals(core, handles.first.getCore());
        assertEquals(core, handles.second.getCore());

        SharedBufferHandle handle = core.createSharedBuffer(null, 100);
        SharedBufferHandle handle2 = handle.duplicate(null);
        addHandleToClose(handle);
        addHandleToClose(handle2);
        assertEquals(core, handle.getCore());
        assertEquals(core, handle2.getCore());
    }

    private static void createAndCloseMessagePipe(MessagePipeHandle.CreateOptions options) {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(options);
        handles.first.close();
        handles.second.close();
    }

    /**
     * Testing {@link MessagePipeHandle} creation.
     */
    @SmallTest
    public void testMessagePipeCreation() {
        // Test creation with null options.
        createAndCloseMessagePipe(null);
        // Test creation with default options.
        createAndCloseMessagePipe(new MessagePipeHandle.CreateOptions());
    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeEmpty() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
        addHandlePairToClose(handles);

        // Testing read on an empty pipe.
        ResultAnd<MessagePipeHandle.ReadMessageResult> readResult =
                handles.first.readMessage(MessagePipeHandle.ReadFlags.NONE);
        assertEquals(MojoResult.SHOULD_WAIT, readResult.getMojoResult());

        handles.first.close();
        handles.second.close();
    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeSend() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
        addHandlePairToClose(handles);

        checkSendingMessage(handles.first, handles.second);
        checkSendingMessage(handles.second, handles.first);
    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeSendHandles() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
        Pair<MessagePipeHandle, MessagePipeHandle> handlesToShare = core.createMessagePipe(null);
        addHandlePairToClose(handles);
        addHandlePairToClose(handlesToShare);

        handles.first.writeMessage(null, Collections.<Handle>singletonList(handlesToShare.second),
                MessagePipeHandle.WriteFlags.NONE);
        assertFalse(handlesToShare.second.isValid());
        ResultAnd<MessagePipeHandle.ReadMessageResult> readMessageResult =
                handles.second.readMessage(MessagePipeHandle.ReadFlags.NONE);
        assertEquals(1, readMessageResult.getValue().mHandles.size());
        MessagePipeHandle newHandle =
                readMessageResult.getValue().mHandles.get(0).toMessagePipeHandle();
        addHandleToClose(newHandle);
        assertTrue(newHandle.isValid());
        checkSendingMessage(handlesToShare.first, newHandle);
        checkSendingMessage(newHandle, handlesToShare.first);
    }

    private static void createAndCloseDataPipe(DataPipe.CreateOptions options) {
        Core core = CoreImpl.getInstance();
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles =
                core.createDataPipe(options);
        handles.first.close();
        handles.second.close();
    }

    /**
     * Testing {@link DataPipe}.
     */
    @SmallTest
    public void testDataPipeCreation() {
        // Create datapipe with null options.
        createAndCloseDataPipe(null);
        DataPipe.CreateOptions options = new DataPipe.CreateOptions();
        // Create datapipe with element size set.
        options.setElementNumBytes(24);
        createAndCloseDataPipe(options);
        // Create datapipe with capacity set.
        options.setCapacityNumBytes(1024 * options.getElementNumBytes());
        createAndCloseDataPipe(options);
    }

    /**
     * Testing {@link DataPipe}.
     */
    @SmallTest
    public void testDataPipeSend() {
        Core core = CoreImpl.getInstance();

        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(null);
        addHandlePairToClose(handles);

        checkSendingData(handles.first, handles.second);
    }

    /**
     * Testing {@link DataPipe}.
     */
    @SmallTest
    public void testDataPipeTwoPhaseSend() {
        Random random = new Random();
        Core core = CoreImpl.getInstance();
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(null);
        addHandlePairToClose(handles);

        // Writing a random 8 bytes message.
        byte[] bytes = new byte[8];
        random.nextBytes(bytes);
        ByteBuffer buffer = handles.first.beginWriteData(bytes.length, DataPipe.WriteFlags.NONE);
        assertTrue(buffer.capacity() >= bytes.length);
        buffer.put(bytes);
        handles.first.endWriteData(bytes.length);

        // Read into a buffer.
        ByteBuffer receiveBuffer =
                handles.second.beginReadData(bytes.length, DataPipe.ReadFlags.NONE);
        assertEquals(0, receiveBuffer.position());
        assertEquals(bytes.length, receiveBuffer.limit());
        byte[] receivedBytes = new byte[bytes.length];
        receiveBuffer.get(receivedBytes);
        assertTrue(Arrays.equals(bytes, receivedBytes));
        handles.second.endReadData(bytes.length);
    }

    /**
     * Testing {@link DataPipe}.
     */
    @SmallTest
    public void testDataPipeDiscard() {
        Random random = new Random();
        Core core = CoreImpl.getInstance();
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(null);
        addHandlePairToClose(handles);

        // Writing a random 8 bytes message.
        byte[] bytes = new byte[8];
        random.nextBytes(bytes);
        ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
        buffer.put(bytes);
        ResultAnd<Integer> result = handles.first.writeData(buffer, DataPipe.WriteFlags.NONE);
        assertEquals(MojoResult.OK, result.getMojoResult());
        assertEquals(bytes.length, result.getValue().intValue());

        // Discard bytes.
        final int nbBytesToDiscard = 4;
        assertEquals(nbBytesToDiscard,
                handles.second.discardData(nbBytesToDiscard, DataPipe.ReadFlags.NONE));

        // Read into a buffer.
        ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(bytes.length - nbBytesToDiscard);
        ResultAnd<Integer> readResult =
                handles.second.readData(receiveBuffer, DataPipe.ReadFlags.NONE);
        assertEquals(MojoResult.OK, readResult.getMojoResult());
        assertEquals(bytes.length - nbBytesToDiscard, readResult.getValue().intValue());
        assertEquals(0, receiveBuffer.position());
        assertEquals(bytes.length - nbBytesToDiscard, receiveBuffer.limit());
        byte[] receivedBytes = new byte[bytes.length - nbBytesToDiscard];
        receiveBuffer.get(receivedBytes);
        assertTrue(Arrays.equals(
                Arrays.copyOfRange(bytes, nbBytesToDiscard, bytes.length), receivedBytes));
    }

    /**
     * Testing {@link SharedBufferHandle}.
     */
    @SmallTest
    public void testSharedBufferCreation() {
        Core core = CoreImpl.getInstance();
        // Test creation with empty options.
        core.createSharedBuffer(null, 8).close();
        // Test creation with default options.
        core.createSharedBuffer(new SharedBufferHandle.CreateOptions(), 8).close();
    }

    /**
     * Testing {@link SharedBufferHandle}.
     */
    @SmallTest
    public void testSharedBufferDuplication() {
        Core core = CoreImpl.getInstance();
        SharedBufferHandle handle = core.createSharedBuffer(null, 8);
        addHandleToClose(handle);

        // Test duplication with empty options.
        handle.duplicate(null).close();
        // Test creation with default options.
        handle.duplicate(new SharedBufferHandle.DuplicateOptions()).close();
    }

    /**
     * Testing {@link SharedBufferHandle}.
     */
    @SmallTest
    public void testSharedBufferSending() {
        Core core = CoreImpl.getInstance();
        SharedBufferHandle handle = core.createSharedBuffer(null, 8);
        addHandleToClose(handle);
        SharedBufferHandle newHandle = handle.duplicate(null);
        addHandleToClose(newHandle);

        checkSharing(handle, newHandle);
        checkSharing(newHandle, handle);
    }

    /**
     * Testing that invalid handle can be used with this implementation.
     */
    @SmallTest
    public void testInvalidHandle() {
        Core core = CoreImpl.getInstance();
        Handle handle = InvalidHandle.INSTANCE;

        // Checking sending an invalid handle. Should result in an ABORTED
        // exception.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
        addHandlePairToClose(handles);
        try {
            handles.first.writeMessage(null, Collections.<Handle>singletonList(handle),
                    MessagePipeHandle.WriteFlags.NONE);
            fail();
        } catch (MojoException e) {
            assertEquals(MojoResult.ABORTED, e.getMojoResult());
        }
    }

    /**
     * Testing the pass method on message pipes.
     */
    @SmallTest
    public void testMessagePipeHandlePass() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
        addHandlePairToClose(handles);

        assertTrue(handles.first.isValid());
        MessagePipeHandle handleClone = handles.first.pass();

        addHandleToClose(handleClone);

        assertFalse(handles.first.isValid());
        assertTrue(handleClone.isValid());
        checkSendingMessage(handleClone, handles.second);
        checkSendingMessage(handles.second, handleClone);
    }

    /**
     * Testing the pass method on data pipes.
     */
    @SmallTest
    public void testDataPipeHandlePass() {
        Core core = CoreImpl.getInstance();
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(null);
        addHandlePairToClose(handles);

        DataPipe.ProducerHandle producerClone = handles.first.pass();
        DataPipe.ConsumerHandle consumerClone = handles.second.pass();

        addHandleToClose(producerClone);
        addHandleToClose(consumerClone);

        assertFalse(handles.first.isValid());
        assertFalse(handles.second.isValid());
        assertTrue(producerClone.isValid());
        assertTrue(consumerClone.isValid());
        checkSendingData(producerClone, consumerClone);
    }

    /**
     * Testing the pass method on shared buffers.
     */
    @SmallTest
    public void testSharedBufferPass() {
        Core core = CoreImpl.getInstance();
        SharedBufferHandle handle = core.createSharedBuffer(null, 8);
        addHandleToClose(handle);
        SharedBufferHandle newHandle = handle.duplicate(null);
        addHandleToClose(newHandle);

        SharedBufferHandle handleClone = handle.pass();
        SharedBufferHandle newHandleClone = newHandle.pass();

        addHandleToClose(handleClone);
        addHandleToClose(newHandleClone);

        assertFalse(handle.isValid());
        assertTrue(handleClone.isValid());
        checkSharing(handleClone, newHandleClone);
        checkSharing(newHandleClone, handleClone);
    }

    /**
     * esting handle conversion to native and back.
     */
    @SmallTest
    public void testHandleConversion() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
        addHandlePairToClose(handles);

        MessagePipeHandle converted =
                core.acquireNativeHandle(handles.first.releaseNativeHandle()).toMessagePipeHandle();
        addHandleToClose(converted);

        assertFalse(handles.first.isValid());

        checkSendingMessage(converted, handles.second);
        checkSendingMessage(handles.second, converted);
    }
}
