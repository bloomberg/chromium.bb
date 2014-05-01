// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.mojo.system.Core.WaitFlags;
import org.chromium.mojo.system.Core.WaitManyResult;
import org.chromium.mojo.system.MessagePipeHandle.ReadFlags;
import org.chromium.mojo.system.MessagePipeHandle.ReadMessageResult;
import org.chromium.mojo.system.MessagePipeHandle.WriteFlags;
import org.chromium.mojo.system.SharedBufferHandle.MapFlags;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/**
 * Testing the core API.
 */
@JNINamespace("mojo::android")
public class CoreTest extends InstrumentationTestCase {

    private static final ScheduledExecutorService WORKER =
            Executors.newSingleThreadScheduledExecutor();

    /**
     * @see junit.framework.TestCase#setUp()
     */
    @Override
    protected void setUp() throws Exception {
        LibraryLoader.ensureInitialized();
        nativeInitApplicationContext(getInstrumentation().getTargetContext());
    }

    /**
     * Runnable that will close the given handle.
     */
    private static class CloseHandle implements Runnable {
        private Handle mHandle;

        CloseHandle(Handle handle) {
            mHandle = handle;
        }

        @Override
        public void run() {
            mHandle.close();
        }
    }

    private static void checkSendingMessage(MessagePipeHandle in, MessagePipeHandle out) {
        Random random = new Random();

        // Writing a random 8 bytes message.
        byte[] bytes = new byte[8];
        random.nextBytes(bytes);
        ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
        buffer.put(bytes);
        in.writeMessage(buffer, null, MessagePipeHandle.WriteFlags.none());

        // Try to read into a small buffer.
        ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(bytes.length / 2);
        MessagePipeHandle.ReadMessageResult result = out.readMessage(receiveBuffer, 0,
                MessagePipeHandle.ReadFlags.none());
        assertFalse(result.getWasMessageRead());
        assertEquals(bytes.length, result.getMessageSize());
        assertEquals(0, result.getHandlesCount());

        // Read into a correct buffer.
        receiveBuffer = ByteBuffer.allocateDirect(bytes.length);
        result = out.readMessage(receiveBuffer, 0,
                MessagePipeHandle.ReadFlags.none());
        assertTrue(result.getWasMessageRead());
        assertEquals(bytes.length, result.getMessageSize());
        assertEquals(0, result.getHandlesCount());
        assertEquals(0, receiveBuffer.position());
        assertEquals(result.getMessageSize(), receiveBuffer.limit());
        byte[] receivedBytes = new byte[result.getMessageSize()];
        receiveBuffer.get(receivedBytes);
        assertTrue(Arrays.equals(bytes, receivedBytes));

    }

    /**
     * Testing {@link Core#waitMany(List, long)}.
     */
    @SmallTest
    public void testWaitMany() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();

        try {
            List<Pair<Handle, WaitFlags>> handlesToWaitOn = new ArrayList<
                    Pair<Handle, WaitFlags>>();

            handlesToWaitOn.add(
                    new Pair<Handle, WaitFlags>(handles.second,
                            WaitFlags.none().setReadable(true)));
            handlesToWaitOn.add(
                    new Pair<Handle, WaitFlags>(handles.first, WaitFlags.none().setWritable(true)));
            WaitManyResult result = core.waitMany(handlesToWaitOn, 0);
            assertEquals(MojoResult.OK, result.getMojoResult());
            assertEquals(1, result.getHandleIndex());

            handlesToWaitOn.clear();
            handlesToWaitOn.add(
                    new Pair<Handle, WaitFlags>(handles.first, WaitFlags.none().setWritable(true)));
            handlesToWaitOn.add(
                    new Pair<Handle, WaitFlags>(handles.second,
                            WaitFlags.none().setReadable(true)));
            result = core.waitMany(handlesToWaitOn, 0);
            assertEquals(MojoResult.OK, result.getMojoResult());
            assertEquals(0, result.getHandleIndex());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeEmpty() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();

        try {
            // Testing wait.
            assertEquals(MojoResult.OK, handles.first.wait(WaitFlags.all(), 0));
            assertEquals(MojoResult.OK, handles.first.wait(WaitFlags.none().setWritable(true), 0));
            assertEquals(MojoResult.DEADLINE_EXCEEDED,
                    handles.first.wait(WaitFlags.none().setReadable(true), 0));

            // Testing read on an empty pipe.
            boolean exception = false;
            try {
                handles.first.readMessage(null, 0, MessagePipeHandle.ReadFlags.none());
            } catch (MojoException e) {
                assertEquals(MojoResult.SHOULD_WAIT, e.getMojoResult());
                exception = true;
            }
            assertTrue(exception);

            // Closing a pipe while waiting.
            WORKER.schedule(new CloseHandle(handles.first), 10, TimeUnit.MILLISECONDS);
            assertEquals(MojoResult.CANCELLED,
                    handles.first.wait(WaitFlags.none().setReadable(true), 1000000L));
        } finally {
            handles.first.close();
            handles.second.close();
        }

        handles = core.createMessagePipe();

        try {
            // Closing the other pipe while waiting.
            WORKER.schedule(new CloseHandle(handles.first), 10, TimeUnit.MILLISECONDS);
            assertEquals(MojoResult.FAILED_PRECONDITION,
                    handles.second.wait(WaitFlags.none().setReadable(true), 1000000L));

            // Waiting on a closed pipe.
            assertEquals(MojoResult.FAILED_PRECONDITION,
                    handles.second.wait(WaitFlags.none().setReadable(true), 0));
            assertEquals(MojoResult.FAILED_PRECONDITION,
                    handles.second.wait(WaitFlags.none().setWritable(true), 0));
        } finally {
            handles.first.close();
            handles.second.close();
        }

    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeSend() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();

        try {
            checkSendingMessage(handles.first, handles.second);
            checkSendingMessage(handles.second, handles.first);
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeReceiveOnSmallBuffer() {
        Random random = new Random();
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();

        try {
            // Writing a random 8 bytes message.
            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
            buffer.put(bytes);
            handles.first.writeMessage(buffer, null, MessagePipeHandle.WriteFlags.none());

            ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(1);
            MessagePipeHandle.ReadMessageResult result = handles.second.readMessage(receiveBuffer,
                    0,
                    MessagePipeHandle.ReadFlags.none());
            assertFalse(result.getWasMessageRead());
            assertEquals(bytes.length, result.getMessageSize());
            assertEquals(0, result.getHandlesCount());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link MessagePipeHandle}.
     */
    @SmallTest
    public void testMessagePipeSendHandles() {
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        Pair<MessagePipeHandle, MessagePipeHandle> handlesToShare = core.createMessagePipe();

        try {
            handles.first.writeMessage(null,
                    Collections.<Handle> singletonList(handlesToShare.second),
                    WriteFlags.none());
            assertFalse(handlesToShare.second.isValid());
            ReadMessageResult readMessageResult = handles.second.readMessage(null, 1,
                    ReadFlags.none());
            assertEquals(1, readMessageResult.getHandlesCount());
            MessagePipeHandle newHandle = readMessageResult.getHandles().get(0)
                    .toMessagePipeHandle();
            try {
                assertTrue(newHandle.isValid());
                checkSendingMessage(handlesToShare.first, newHandle);
                checkSendingMessage(newHandle, handlesToShare.first);
            } finally {
                newHandle.close();
            }
        } finally {
            handles.first.close();
            handles.second.close();
            handlesToShare.first.close();
            handlesToShare.second.close();
        }
    }

    private static void createAndCloseDataPipe(DataPipe.CreateOptions options) {
        Core core = CoreImpl.getInstance();
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(
                options);
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
        // Create datapipe with a flag set.
        options.getFlags().setMayDiscard(true);
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
        Random random = new Random();

        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(null);
        try {
            // Writing a random 8 bytes message.
            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
            buffer.put(bytes);
            int result = handles.first.writeData(buffer, DataPipe.WriteFlags.none());
            assertEquals(bytes.length, result);

            // Query number of bytes available.
            result = handles.second.readData(null,
                    DataPipe.ReadFlags.none().query(true));
            assertEquals(bytes.length, result);

            // Read into a buffer.
            ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(bytes.length);
            result = handles.second.readData(receiveBuffer,
                    DataPipe.ReadFlags.none());
            assertEquals(bytes.length, result);
            assertEquals(0, receiveBuffer.position());
            assertEquals(bytes.length, receiveBuffer.limit());
            byte[] receivedBytes = new byte[bytes.length];
            receiveBuffer.get(receivedBytes);
            assertTrue(Arrays.equals(bytes, receivedBytes));
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link DataPipe}.
     */
    @SmallTest
    public void testDataPipeTwoPhaseSend() {
        Random random = new Random();
        Core core = CoreImpl.getInstance();
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(null);

        try {
            // Writing a random 8 bytes message.
            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            ByteBuffer buffer = handles.first.beginWriteData(bytes.length,
                    DataPipe.WriteFlags.none());
            assertTrue(buffer.capacity() >= bytes.length);
            buffer.put(bytes);
            handles.first.endWriteData(bytes.length);

            // Read into a buffer.
            ByteBuffer receiveBuffer = handles.second.beginReadData(bytes.length,
                    DataPipe.ReadFlags.none());
            assertEquals(0, receiveBuffer.position());
            assertEquals(bytes.length, receiveBuffer.limit());
            byte[] receivedBytes = new byte[bytes.length];
            receiveBuffer.get(receivedBytes);
            assertTrue(Arrays.equals(bytes, receivedBytes));
            handles.second.endReadData(bytes.length);
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    /**
     * Testing {@link DataPipe}.
     */
    @SmallTest
    public void testDataPipeDiscard() {
        Random random = new Random();
        Core core = CoreImpl.getInstance();
        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> handles = core.createDataPipe(null);

        try {
            // Writing a random 8 bytes message.
            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length);
            buffer.put(bytes);
            int result = handles.first.writeData(buffer, DataPipe.WriteFlags.none());
            assertEquals(bytes.length, result);

            // Discard bytes.
            final int nbBytesToDiscard = 4;
            assertEquals(nbBytesToDiscard,
                    handles.second.discardData(nbBytesToDiscard, DataPipe.ReadFlags.none()));

            // Read into a buffer.
            ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(bytes.length - nbBytesToDiscard);
            result = handles.second.readData(receiveBuffer,
                    DataPipe.ReadFlags.none());
            assertEquals(bytes.length - nbBytesToDiscard, result);
            assertEquals(0, receiveBuffer.position());
            assertEquals(bytes.length - nbBytesToDiscard, receiveBuffer.limit());
            byte[] receivedBytes = new byte[bytes.length - nbBytesToDiscard];
            receiveBuffer.get(receivedBytes);
            assertTrue(Arrays.equals(Arrays.copyOfRange(bytes, nbBytesToDiscard, bytes.length),
                    receivedBytes));
        } finally {
            handles.first.close();
            handles.second.close();
        }
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
        core.createSharedBuffer(new SharedBufferHandle.CreateOptions(), 8);
    }

    /**
     * Testing {@link SharedBufferHandle}.
     */
    @SmallTest
    public void testSharedBufferDuplication() {
        Core core = CoreImpl.getInstance();
        SharedBufferHandle handle = core.createSharedBuffer(null, 8);
        try {
            // Test duplication with empty options.
            handle.duplicate(null).close();
            // Test creation with default options.
            handle.duplicate(new SharedBufferHandle.DuplicateOptions()).close();
        } finally {
            handle.close();
        }
    }

    /**
     * Testing {@link SharedBufferHandle}.
     */
    @SmallTest
    public void testSharedBufferSending() {
        Random random = new Random();
        Core core = CoreImpl.getInstance();
        SharedBufferHandle handle = core.createSharedBuffer(null, 8);
        SharedBufferHandle newHandle = handle.duplicate(null);

        try {
            ByteBuffer buffer1 = handle.map(0, 8, MapFlags.none());
            assertEquals(8, buffer1.capacity());
            ByteBuffer buffer2 = newHandle.map(0, 8, MapFlags.none());
            assertEquals(8, buffer2.capacity());

            byte[] bytes = new byte[8];
            random.nextBytes(bytes);
            buffer1.put(bytes);

            byte[] receivedBytes = new byte[bytes.length];
            buffer2.get(receivedBytes);

            assertTrue(Arrays.equals(bytes, receivedBytes));

            handle.unmap(buffer1);
            newHandle.unmap(buffer2);
        } finally {
            handle.close();
            newHandle.close();
        }
    }

    /**
     * Testing that invalid handle can be used with this implementation.
     */
    @SmallTest
    public void testInvalidHandle() {
        Core core = CoreImpl.getInstance();
        Handle handle = new InvalidHandle();

        // Checking wait.
        boolean exception = false;
        try {
            core.wait(handle, WaitFlags.all(), 0);
        } catch (MojoException e) {
            assertEquals(MojoResult.INVALID_ARGUMENT, e.getMojoResult());
            exception = true;
        }
        assertTrue(exception);

        // Checking waitMany.
        exception = false;
        try {
            List<Pair<Handle, WaitFlags>> handles = new ArrayList<Pair<Handle, WaitFlags>>();
            handles.add(Pair.create(handle, WaitFlags.all()));
            core.waitMany(handles, 0);
        } catch (MojoException e) {
            assertEquals(MojoResult.INVALID_ARGUMENT, e.getMojoResult());
            exception = true;
        }
        assertTrue(exception);

        // Checking sending an invalid handle.
        // Until the behavior is changed on the C++ side, handle gracefully 2 different use case:
        // - Receive a INVALID_ARGUMENT exception
        // - Receive an invalid handle on the other side.
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe();
        try {
            handles.first.writeMessage(null,
                    Collections.<Handle> singletonList(handle),
                    WriteFlags.none());
            ReadMessageResult readMessageResult = handles.second.readMessage(null, 1,
                    ReadFlags.none());
            assertEquals(1, readMessageResult.getHandlesCount());
            assertFalse(readMessageResult.getHandles().get(0).isValid());
        } catch (MojoException e) {
            assertEquals(MojoResult.INVALID_ARGUMENT, e.getMojoResult());
        } finally {
            handles.first.close();
            handles.second.close();
        }
    }

    private native void nativeInitApplicationContext(Context context);
}
