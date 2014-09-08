// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system.impl;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.mojo.system.AsyncWaiter;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.DataPipe;
import org.chromium.mojo.system.DataPipe.ConsumerHandle;
import org.chromium.mojo.system.DataPipe.ProducerHandle;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.SharedBufferHandle;
import org.chromium.mojo.system.SharedBufferHandle.DuplicateOptions;
import org.chromium.mojo.system.SharedBufferHandle.MapFlags;
import org.chromium.mojo.system.UntypedHandle;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of {@link Core}.
 */
@JNINamespace("mojo::android")
public class CoreImpl implements Core, AsyncWaiter {

    /**
     * Discard flag for the |MojoReadData| operation.
     */
    private static final int MOJO_READ_DATA_FLAG_DISCARD = 1 << 1;

    /**
     * the size of a handle, in bytes.
     */
    private static final int HANDLE_SIZE = 4;

    /**
     * the size of a flag, in bytes.
     */
    private static final int FLAG_SIZE = 4;

    /**
     * The mojo handle for an invalid handle.
     */
    static final int INVALID_HANDLE = 0;

    private static class LazyHolder {
        private static final Core INSTANCE = new CoreImpl();
    }

    /**
     * @return the instance.
     */
    public static Core getInstance() {
        return LazyHolder.INSTANCE;
    }

    private CoreImpl() {
    }

    /**
     * @see Core#getTimeTicksNow()
     */
    @Override
    public long getTimeTicksNow() {
        return nativeGetTimeTicksNow();
    }

    /**
     * @see Core#waitMany(List, long)
     */
    @Override
    public WaitManyResult waitMany(List<Pair<Handle, HandleSignals>> handles, long deadline) {
        // Allocate a direct buffer to allow native code not to reach back to java. Buffer will
        // contain all mojo handles, followed by all flags values.
        ByteBuffer buffer = allocateDirectBuffer(handles.size() * 8);
        int index = 0;
        for (Pair<Handle, HandleSignals> handle : handles) {
            buffer.putInt(HANDLE_SIZE * index, getMojoHandle(handle.first));
            buffer.putInt(HANDLE_SIZE * handles.size() + FLAG_SIZE * index,
                    handle.second.getFlags());
            index++;
        }
        int code = nativeWaitMany(buffer, deadline);
        WaitManyResult result = new WaitManyResult();
        // If result is greater than 0, result is the indexed of the available handle. To make sure
        // it cannot be misinterpreted, set handleIndex to a negative number in case of error.
        result.setHandleIndex(code);
        result.setMojoResult(filterMojoResultForWait(code));
        return result;
    }

    /**
     * @see Core#wait(Handle, HandleSignals, long)
     */
    @Override
    public int wait(Handle handle, HandleSignals signals, long deadline) {
        return filterMojoResultForWait(nativeWait(getMojoHandle(handle),
                signals.getFlags(), deadline));
    }

    /**
     * @see Core#createMessagePipe(MessagePipeHandle.CreateOptions)
     */
    @Override
    public Pair<MessagePipeHandle, MessagePipeHandle> createMessagePipe(
            MessagePipeHandle.CreateOptions options) {
        ByteBuffer optionsBuffer = null;
        if (options != null) {
            optionsBuffer = allocateDirectBuffer(8);
            optionsBuffer.putInt(0, 8);
            optionsBuffer.putInt(4, options.getFlags().getFlags());
        }
        NativeCreationResult result = nativeCreateMessagePipe(optionsBuffer);
        if (result.getMojoResult() != MojoResult.OK) {
            throw new MojoException(result.getMojoResult());
        }
        return Pair.<MessagePipeHandle, MessagePipeHandle> create(
                new MessagePipeHandleImpl(this, result.getMojoHandle1()),
                new MessagePipeHandleImpl(this, result.getMojoHandle2()));
    }

    /**
     * @see Core#createDataPipe(DataPipe.CreateOptions)
     */
    @Override
    public Pair<ProducerHandle, ConsumerHandle> createDataPipe(DataPipe.CreateOptions options) {
        ByteBuffer optionsBuffer = null;
        if (options != null) {
            optionsBuffer = allocateDirectBuffer(16);
            optionsBuffer.putInt(0, 16);
            optionsBuffer.putInt(4, options.getFlags().getFlags());
            optionsBuffer.putInt(8, options.getElementNumBytes());
            optionsBuffer.putInt(12, options.getCapacityNumBytes());
        }
        NativeCreationResult result = nativeCreateDataPipe(optionsBuffer);
        if (result.getMojoResult() != MojoResult.OK) {
            throw new MojoException(result.getMojoResult());
        }
        return Pair.<ProducerHandle, ConsumerHandle> create(
                new DataPipeProducerHandleImpl(this, result.getMojoHandle1()),
                new DataPipeConsumerHandleImpl(this, result.getMojoHandle2()));
    }

    /**
     * @see Core#createSharedBuffer(SharedBufferHandle.CreateOptions, long)
     */
    @Override
    public SharedBufferHandle createSharedBuffer(
            SharedBufferHandle.CreateOptions options, long numBytes) {
        ByteBuffer optionsBuffer = null;
        if (options != null) {
            optionsBuffer = allocateDirectBuffer(8);
            optionsBuffer.putInt(0, 8);
            optionsBuffer.putInt(4, options.getFlags().getFlags());
        }
        NativeCreationResult result = nativeCreateSharedBuffer(optionsBuffer, numBytes);
        if (result.getMojoResult() != MojoResult.OK) {
            throw new MojoException(result.getMojoResult());
        }
        assert result.getMojoHandle2() == 0;
        return new SharedBufferHandleImpl(this, result.getMojoHandle1());
    }

    /**
     * @see org.chromium.mojo.system.Core#acquireNativeHandle(int)
     */
    @Override
    public UntypedHandle acquireNativeHandle(int handle) {
        return new UntypedHandleImpl(this, handle);
    }

    /**
     * @see Core#getDefaultAsyncWaiter()
     */
    @Override
    public AsyncWaiter getDefaultAsyncWaiter() {
        return this;
    }

    /**
     * @see AsyncWaiter#asyncWait(Handle, Core.HandleSignals, long, Callback)
     */
    @Override
    public Cancellable asyncWait(Handle handle, HandleSignals signals, long deadline,
            Callback callback) {
        return nativeAsyncWait(getMojoHandle(handle), signals.getFlags(), deadline, callback);
    }

    int closeWithResult(int mojoHandle) {
        return nativeClose(mojoHandle);
    }

    void close(int mojoHandle) {
        int mojoResult = nativeClose(mojoHandle);
        if (mojoResult != MojoResult.OK) {
            throw new MojoException(mojoResult);
        }
    }

    /**
     * @see MessagePipeHandle#writeMessage(ByteBuffer, List, MessagePipeHandle.WriteFlags)
     */
    void writeMessage(MessagePipeHandleImpl pipeHandle, ByteBuffer bytes,
            List<? extends Handle> handles, MessagePipeHandle.WriteFlags flags) {
        ByteBuffer handlesBuffer = null;
        if (handles != null && !handles.isEmpty()) {
            handlesBuffer = allocateDirectBuffer(handles.size() * HANDLE_SIZE);
            for (Handle handle : handles) {
                handlesBuffer.putInt(getMojoHandle(handle));
            }
            handlesBuffer.position(0);
        }
        int mojoResult = nativeWriteMessage(pipeHandle.getMojoHandle(), bytes,
                bytes == null ? 0 : bytes.limit(), handlesBuffer,
                flags.getFlags());
        if (mojoResult != MojoResult.OK) {
            throw new MojoException(mojoResult);
        }
        // Success means the handles have been invalidated.
        if (handles != null) {
            for (Handle handle : handles) {
                if (handle.isValid()) {
                    ((HandleBase) handle).invalidateHandle();
                }
            }
        }
    }

    /**
     * @see MessagePipeHandle#readMessage(ByteBuffer, int, MessagePipeHandle.ReadFlags)
     */
    MessagePipeHandle.ReadMessageResult readMessage(MessagePipeHandleImpl handle,
            ByteBuffer bytes, int maxNumberOfHandles,
            MessagePipeHandle.ReadFlags flags) {
        ByteBuffer handlesBuffer = null;
        if (maxNumberOfHandles > 0) {
            handlesBuffer = allocateDirectBuffer(maxNumberOfHandles * HANDLE_SIZE);
        }
        MessagePipeHandle.ReadMessageResult result = nativeReadMessage(
                handle.getMojoHandle(), bytes, handlesBuffer, flags.getFlags());
        if (result.getMojoResult() != MojoResult.OK &&
                result.getMojoResult() != MojoResult.RESOURCE_EXHAUSTED &&
                result.getMojoResult() != MojoResult.SHOULD_WAIT) {
            throw new MojoException(result.getMojoResult());
        }

        if (result.getMojoResult() == MojoResult.OK) {
            if (bytes != null) {
                bytes.position(0);
                bytes.limit(result.getMessageSize());
            }

            List<UntypedHandle> handles = new ArrayList<UntypedHandle>(
                    result.getHandlesCount());
            for (int i = 0; i < result.getHandlesCount(); ++i) {
                int mojoHandle = handlesBuffer.getInt(HANDLE_SIZE * i);
                handles.add(new UntypedHandleImpl(this, mojoHandle));
            }
            result.setHandles(handles);
        }
        return result;
    }

    /**
     * @see ConsumerHandle#discardData(int, DataPipe.ReadFlags)
     */
    int discardData(DataPipeConsumerHandleImpl handle, int numBytes,
            DataPipe.ReadFlags flags) {
        int result = nativeReadData(handle.getMojoHandle(), null, numBytes,
                flags.getFlags() | MOJO_READ_DATA_FLAG_DISCARD);
        if (result < 0) {
            throw new MojoException(result);
        }
        return result;
    }

    /**
     * @see ConsumerHandle#readData(ByteBuffer, DataPipe.ReadFlags)
     */
    int readData(DataPipeConsumerHandleImpl handle, ByteBuffer elements,
            DataPipe.ReadFlags flags) {
        int result = nativeReadData(handle.getMojoHandle(), elements,
                elements == null ? 0 : elements.capacity(),
                flags.getFlags());
        if (result < 0) {
            throw new MojoException(result);
        }
        if (elements != null) {
            elements.limit(result);
        }
        return result;
    }

    /**
     * @see ConsumerHandle#beginReadData(int, DataPipe.ReadFlags)
     */
    ByteBuffer beginReadData(DataPipeConsumerHandleImpl handle,
            int numBytes, DataPipe.ReadFlags flags) {
        NativeCodeAndBufferResult result = nativeBeginReadData(
                handle.getMojoHandle(),
                numBytes,
                flags.getFlags());
        if (result.getMojoResult() != MojoResult.OK) {
            throw new MojoException(result.getMojoResult());
        }
        return result.getBuffer().asReadOnlyBuffer();
    }

    /**
     * @see ConsumerHandle#endReadData(int)
     */
    void endReadData(DataPipeConsumerHandleImpl handle,
            int numBytesRead) {
        int result = nativeEndReadData(handle.getMojoHandle(), numBytesRead);
        if (result != MojoResult.OK) {
            throw new MojoException(result);
        }
    }

    /**
     * @see ProducerHandle#writeData(ByteBuffer, DataPipe.WriteFlags)
     */
    int writeData(DataPipeProducerHandleImpl handle, ByteBuffer elements,
            DataPipe.WriteFlags flags) {
        return nativeWriteData(handle.getMojoHandle(), elements, elements.limit(),
                flags.getFlags());
    }

    /**
     * @see ProducerHandle#beginWriteData(int, DataPipe.WriteFlags)
     */
    ByteBuffer beginWriteData(DataPipeProducerHandleImpl handle,
            int numBytes, DataPipe.WriteFlags flags) {
        NativeCodeAndBufferResult result = nativeBeginWriteData(
                handle.getMojoHandle(),
                numBytes,
                flags.getFlags());
        if (result.getMojoResult() != MojoResult.OK) {
            throw new MojoException(result.getMojoResult());
        }
        return result.getBuffer();
    }

    /**
     * @see ProducerHandle#endWriteData(int)
     */
    void endWriteData(DataPipeProducerHandleImpl handle,
            int numBytesWritten) {
        int result = nativeEndWriteData(handle.getMojoHandle(), numBytesWritten);
        if (result != MojoResult.OK) {
            throw new MojoException(result);
        }
    }

    /**
     * @see SharedBufferHandle#duplicate(DuplicateOptions)
     */
    SharedBufferHandle duplicate(SharedBufferHandleImpl handle,
            DuplicateOptions options) {
        ByteBuffer optionsBuffer = null;
        if (options != null) {
            optionsBuffer = allocateDirectBuffer(8);
            optionsBuffer.putInt(0, 8);
            optionsBuffer.putInt(4, options.getFlags().getFlags());
        }
        NativeCreationResult result = nativeDuplicate(handle.getMojoHandle(),
                optionsBuffer);
        if (result.getMojoResult() != MojoResult.OK) {
            throw new MojoException(result.getMojoResult());
        }
        assert result.getMojoHandle2() == 0;
        return new SharedBufferHandleImpl(this, result.getMojoHandle1());
    }

    /**
     * @see SharedBufferHandle#map(long, long, MapFlags)
     */
    ByteBuffer map(SharedBufferHandleImpl handle, long offset, long numBytes,
            MapFlags flags) {
        NativeCodeAndBufferResult result = nativeMap(handle.getMojoHandle(), offset, numBytes,
                flags.getFlags());
        if (result.getMojoResult() != MojoResult.OK) {
            throw new MojoException(result.getMojoResult());
        }
        return result.getBuffer();
    }

    /**
     * @see SharedBufferHandle#unmap(ByteBuffer)
     */
    void unmap(ByteBuffer buffer) {
        int result = nativeUnmap(buffer);
        if (result != MojoResult.OK) {
            throw new MojoException(result);
        }
    }

    /**
     * @return the mojo handle associated to the given handle, considering invalid handles.
     */
    private int getMojoHandle(Handle handle) {
        if (handle.isValid()) {
            return ((HandleBase) handle).getMojoHandle();
        }
        return 0;
    }

    private static boolean isUnrecoverableError(int code) {
        switch (code) {
            case MojoResult.OK:
            case MojoResult.DEADLINE_EXCEEDED:
            case MojoResult.CANCELLED:
            case MojoResult.FAILED_PRECONDITION:
                return false;
            default:
                return true;
        }
    }

    private static int filterMojoResult(int code) {
        if (code >= 0) {
            return MojoResult.OK;
        }
        return code;
    }

    private static int filterMojoResultForWait(int code) {
        int finalCode = filterMojoResult(code);
        if (isUnrecoverableError(finalCode)) {
            throw new MojoException(finalCode);
        }
        return finalCode;
    }

    private static ByteBuffer allocateDirectBuffer(int capacity) {
        ByteBuffer buffer = ByteBuffer.allocateDirect(capacity);
        buffer.order(ByteOrder.nativeOrder());
        return buffer;
    }

    private static class NativeCodeAndBufferResult {
        private int mMojoResult;
        private ByteBuffer mBuffer;

        /**
         * @return the mojoResult
         */
        public int getMojoResult() {
            return mMojoResult;
        }

        /**
         * @param mojoResult the mojoResult to set
         */
        public void setMojoResult(int mojoResult) {
            mMojoResult = mojoResult;
        }

        /**
         * @return the buffer
         */
        public ByteBuffer getBuffer() {
            return mBuffer;
        }

        /**
         * @param buffer the buffer to set
         */
        public void setBuffer(ByteBuffer buffer) {
            mBuffer = buffer;
        }

    }

    /**
     * Implementation of {@link org.chromium.mojo.system.AsyncWaiter.Cancellable}.
     */
    private class AsyncWaiterCancellableImpl implements AsyncWaiter.Cancellable {

        private final long mId;
        private final long mDataPtr;
        private boolean mActive = true;

        private AsyncWaiterCancellableImpl(long id, long dataPtr) {
            this.mId = id;
            this.mDataPtr = dataPtr;
        }

        /**
         * @see org.chromium.mojo.system.AsyncWaiter.Cancellable#cancel()
         */
        @Override
        public void cancel() {
            if (mActive) {
                mActive = false;
                nativeCancelAsyncWait(mId, mDataPtr);
            }
        }

        private boolean isActive() {
            return mActive;
        }

        private void deactivate() {
            mActive = false;
        }
    }

    @CalledByNative
    private AsyncWaiterCancellableImpl newAsyncWaiterCancellableImpl(long id, long dataPtr) {
        return new AsyncWaiterCancellableImpl(id, dataPtr);
    }

    @CalledByNative
    private void onAsyncWaitResult(int mojoResult,
            AsyncWaiter.Callback callback,
            AsyncWaiterCancellableImpl cancellable) {
        if (!cancellable.isActive()) {
            // If cancellable is not active, the user cancelled the wait.
            return;
        }
        cancellable.deactivate();
        int finalCode = filterMojoResult(mojoResult);
        if (isUnrecoverableError(finalCode)) {
            callback.onError(new MojoException(finalCode));
            return;
        }
        callback.onResult(finalCode);
    }

    @CalledByNative
    private static NativeCodeAndBufferResult newNativeCodeAndBufferResult(int mojoResult,
            ByteBuffer buffer) {
        NativeCodeAndBufferResult result = new NativeCodeAndBufferResult();
        result.setMojoResult(mojoResult);
        result.setBuffer(buffer);
        return result;
    }

    @CalledByNative
    private static MessagePipeHandle.ReadMessageResult newReadMessageResult(int mojoResult,
            int messageSize,
            int handlesCount) {
        MessagePipeHandle.ReadMessageResult result = new MessagePipeHandle.ReadMessageResult();
        if (mojoResult >= 0) {
            result.setMojoResult(MojoResult.OK);
        } else {
            result.setMojoResult(mojoResult);
        }
        result.setMessageSize(messageSize);
        result.setHandlesCount(handlesCount);
        return result;
    }

    private static class NativeCreationResult {
        private int mMojoResult;
        private int mMojoHandle1;
        private int mMojoHandle2;

        /**
         * @return the mojoResult
         */
        public int getMojoResult() {
            return mMojoResult;
        }

        /**
         * @param mojoResult the mojoResult to set
         */
        public void setMojoResult(int mojoResult) {
            mMojoResult = mojoResult;
        }

        /**
         * @return the mojoHandle1
         */
        public int getMojoHandle1() {
            return mMojoHandle1;
        }

        /**
         * @param mojoHandle1 the mojoHandle1 to set
         */
        public void setMojoHandle1(int mojoHandle1) {
            mMojoHandle1 = mojoHandle1;
        }

        /**
         * @return the mojoHandle2
         */
        public int getMojoHandle2() {
            return mMojoHandle2;
        }

        /**
         * @param mojoHandle2 the mojoHandle2 to set
         */
        public void setMojoHandle2(int mojoHandle2) {
            mMojoHandle2 = mojoHandle2;
        }
    }

    @CalledByNative
    private static NativeCreationResult newNativeCreationResult(int mojoResult,
            int mojoHandle1, int mojoHandle2) {
        NativeCreationResult result = new NativeCreationResult();
        result.setMojoResult(mojoResult);
        result.setMojoHandle1(mojoHandle1);
        result.setMojoHandle2(mojoHandle2);
        return result;
    }

    private native long nativeGetTimeTicksNow();

    private native int nativeWaitMany(ByteBuffer buffer, long deadline);

    private native NativeCreationResult nativeCreateMessagePipe(ByteBuffer optionsBuffer);

    private native NativeCreationResult nativeCreateDataPipe(ByteBuffer optionsBuffer);

    private native NativeCreationResult nativeCreateSharedBuffer(ByteBuffer optionsBuffer,
            long numBytes);

    private native int nativeClose(int mojoHandle);

    private native int nativeWait(int mojoHandle, int signals, long deadline);

    private native int nativeWriteMessage(int mojoHandle, ByteBuffer bytes, int numBytes,
            ByteBuffer handlesBuffer, int flags);

    private native MessagePipeHandle.ReadMessageResult nativeReadMessage(int mojoHandle,
            ByteBuffer bytes,
            ByteBuffer handlesBuffer,
            int flags);

    private native int nativeReadData(int mojoHandle, ByteBuffer elements, int elementsSize,
            int flags);

    private native NativeCodeAndBufferResult nativeBeginReadData(int mojoHandle, int numBytes,
            int flags);

    private native int nativeEndReadData(int mojoHandle, int numBytesRead);

    private native int nativeWriteData(int mojoHandle, ByteBuffer elements, int limit, int flags);

    private native NativeCodeAndBufferResult nativeBeginWriteData(int mojoHandle, int numBytes,
            int flags);

    private native int nativeEndWriteData(int mojoHandle, int numBytesWritten);

    private native NativeCreationResult nativeDuplicate(int mojoHandle, ByteBuffer optionsBuffer);

    private native NativeCodeAndBufferResult nativeMap(int mojoHandle, long offset, long numBytes,
            int flags);

    private native int nativeUnmap(ByteBuffer buffer);

    private native AsyncWaiterCancellableImpl nativeAsyncWait(int mojoHandle, int signals,
            long deadline, AsyncWaiter.Callback callback);

    private native void nativeCancelAsyncWait(long mId, long dataPtr);
}
