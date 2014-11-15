// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.bindings.Interface.Proxy;
import org.chromium.mojo.bindings.Struct.DataHeader;
import org.chromium.mojo.system.DataPipe;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.InvalidHandle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.SharedBufferHandle;
import org.chromium.mojo.system.UntypedHandle;

import java.nio.ByteOrder;
import java.nio.charset.Charset;

/**
 * A Decoder is a helper class for deserializing a mojo struct. It enables deserialization of basic
 * types from a {@link Message} object at a given offset into it's byte buffer.
 */
public class Decoder {

    /**
     * Helper class to validate the decoded message.
     */
    static final class Validator {

        /**
         * Minimal value for the next handle to deserialize.
         */
        private int mMinNextClaimedHandle = 0;
        /**
         * Minimal value of the start of the next memory to claim.
         */
        private long mMinNextMemory = 0;

        /**
         * The maximal memory accessible.
         */
        private final long mMaxMemory;

        /**
         * The number of handles in the message.
         */
        private final long mNumberOfHandles;

        /**
         * Constructor.
         */
        Validator(long maxMemory, int numberOfHandles) {
            mMaxMemory = maxMemory;
            mNumberOfHandles = numberOfHandles;
        }

        public void claimHandle(int handle) {
            if (handle < mMinNextClaimedHandle) {
                throw new DeserializationException(
                        "Trying to access handle out of order.");
            }
            if (handle >= mNumberOfHandles) {
                throw new DeserializationException("Trying to access non present handle.");
            }
            mMinNextClaimedHandle = handle + 1;
        }

        public void claimMemory(long start, long end) {
            if (start % BindingsHelper.ALIGNMENT != 0) {
                throw new DeserializationException("Incorrect starting alignment: " + start + ".");
            }
            if (start < mMinNextMemory) {
                throw new DeserializationException("Trying to access memory out of order.");
            }
            if (end < start) {
                throw new DeserializationException("Incorrect memory range.");
            }
            if (end > mMaxMemory) {
                throw new DeserializationException("Trying to access out of range memory.");
            }
            mMinNextMemory = BindingsHelper.align(end);
        }
    }

    /**
     * The message to deserialize from.
     */
    private final Message mMessage;

    /**
     * The base offset in the byte buffer.
     */
    private final int mBaseOffset;

    /**
     * Validator for the decoded message.
     */
    private final Validator mValidator;

    /**
     * Constructor.
     *
     * @param message The message to decode.
     */
    public Decoder(Message message) {
        this(message, new Validator(message.getData().limit(), message.getHandles().size()), 0);
    }

    private Decoder(Message message, Validator validator, int baseOffset) {
        mMessage = message;
        mMessage.getData().order(ByteOrder.nativeOrder());
        mBaseOffset = baseOffset;
        mValidator = validator;
    }

    /**
     * Deserializes a {@link DataHeader} at the given offset.
     */
    public DataHeader readDataHeader() {
        // Claim the memory for the header.
        mValidator.claimMemory(mBaseOffset, mBaseOffset + DataHeader.HEADER_SIZE);
        int size = readInt(DataHeader.SIZE_OFFSET);
        int numFields = readInt(DataHeader.NUM_FIELDS_OFFSET);
        if (size < 0) {
            throw new DeserializationException(
                    "Negative size. Unsigned integers are not valid for java.");
        }
        if (numFields < 0) {
            throw new DeserializationException(
                    "Negative number of fields. Unsigned integers are not valid for java.");
        }

        // Claim the remaining memory.
        mValidator.claimMemory(mBaseOffset + DataHeader.HEADER_SIZE, mBaseOffset + size);
        DataHeader res = new DataHeader(size, numFields);
        return res;
    }

    /**
     * Deserializes a {@link DataHeader} at the given offset and checks if it is correct for an
     * array where element have the given size.
     */
    public DataHeader readDataHeaderForPointerArray(int expectedLength) {
        return readDataHeaderForArray(8, expectedLength);
    }

    /**
     * Deserializes a {@link DataHeader} at the given offset and checks if it is correct for a map.
     */
    public void readDataHeaderForMap() {
        DataHeader si = readDataHeader();
        if (si.size != BindingsHelper.MAP_STRUCT_HEADER.size) {
            throw new DeserializationException(
                    "Incorrect header for map. The size is incorrect.");
        }
        if (si.numFields != BindingsHelper.MAP_STRUCT_HEADER.numFields) {
            throw new DeserializationException(
                    "Incorrect header for map. The number of fields is incorrect.");
        }
    }

    /**
     * Deserializes a byte at the given offset.
     */
    public byte readByte(int offset) {
        validateBufferSize(offset, 1);
        return mMessage.getData().get(mBaseOffset + offset);
    }

    /**
     * Deserializes a boolean at the given offset, re-using any partially read byte.
     */
    public boolean readBoolean(int offset, int bit) {
        validateBufferSize(offset, 1);
        return (readByte(offset) & (1 << bit)) != 0;
    }

    /**
     * Deserializes a short at the given offset.
     */
    public short readShort(int offset) {
        validateBufferSize(offset, 2);
        return mMessage.getData().getShort(mBaseOffset + offset);
    }

    /**
     * Deserializes an int at the given offset.
     */
    public int readInt(int offset) {
        validateBufferSize(offset, 4);
        return mMessage.getData().getInt(mBaseOffset + offset);
    }

    /**
     * Deserializes a float at the given offset.
     */
    public float readFloat(int offset) {
        validateBufferSize(offset, 4);
        return mMessage.getData().getFloat(mBaseOffset + offset);
    }

    /**
     * Deserializes a long at the given offset.
     */
    public long readLong(int offset) {
        validateBufferSize(offset, 8);
        return mMessage.getData().getLong(mBaseOffset + offset);
    }

    /**
     * Deserializes a double at the given offset.
     */
    public double readDouble(int offset) {
        validateBufferSize(offset, 8);
        return mMessage.getData().getDouble(mBaseOffset + offset);
    }

    /**
     * Deserializes a pointer at the given offset. Returns a Decoder suitable to decode the content
     * of the pointer.
     */
    public Decoder readPointer(int offset, boolean nullable) {
        int basePosition = mBaseOffset + offset;
        long pointerOffset = readLong(offset);
        if (pointerOffset == 0) {
            if (!nullable) {
                throw new DeserializationException(
                        "Trying to decode null pointer for a non-nullable type.");
            }
            return null;
        }
        int newPosition = (int) (basePosition + pointerOffset);
        // The method |getDecoderAtPosition| will validate that the pointer address is valid.
        return getDecoderAtPosition(newPosition);

    }

    /**
     * Deserializes an array of boolean at the given offset.
     */
    public boolean[] readBooleans(int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForBooleanArray(expectedLength);
        byte[] bytes = new byte[(si.numFields + 7) / BindingsHelper.ALIGNMENT];
        d.mMessage.getData().position(d.mBaseOffset + DataHeader.HEADER_SIZE);
        d.mMessage.getData().get(bytes);
        boolean[] result = new boolean[si.numFields];
        for (int i = 0; i < bytes.length; ++i) {
            for (int j = 0; j < BindingsHelper.ALIGNMENT; ++j) {
                int booleanIndex = i * BindingsHelper.ALIGNMENT + j;
                if (booleanIndex < result.length) {
                    result[booleanIndex] = (bytes[i] & (1 << j)) != 0;
                }
            }
        }
        return result;
    }

    /**
     * Deserializes an array of bytes at the given offset.
     */
    public byte[] readBytes(int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(1, expectedLength);
        byte[] result = new byte[si.numFields];
        d.mMessage.getData().position(d.mBaseOffset + DataHeader.HEADER_SIZE);
        d.mMessage.getData().get(result);
        return result;
    }

    /**
     * Deserializes an array of shorts at the given offset.
     */
    public short[] readShorts(int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(2, expectedLength);
        short[] result = new short[si.numFields];
        d.mMessage.getData().position(d.mBaseOffset + DataHeader.HEADER_SIZE);
        d.mMessage.getData().asShortBuffer().get(result);
        return result;
    }

    /**
     * Deserializes an array of ints at the given offset.
     */
    public int[] readInts(int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(4, expectedLength);
        int[] result = new int[si.numFields];
        d.mMessage.getData().position(d.mBaseOffset + DataHeader.HEADER_SIZE);
        d.mMessage.getData().asIntBuffer().get(result);
        return result;
    }

    /**
     * Deserializes an array of floats at the given offset.
     */
    public float[] readFloats(int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(4, expectedLength);
        float[] result = new float[si.numFields];
        d.mMessage.getData().position(d.mBaseOffset + DataHeader.HEADER_SIZE);
        d.mMessage.getData().asFloatBuffer().get(result);
        return result;
    }

    /**
     * Deserializes an array of longs at the given offset.
     */
    public long[] readLongs(int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(8, expectedLength);
        long[] result = new long[si.numFields];
        d.mMessage.getData().position(d.mBaseOffset + DataHeader.HEADER_SIZE);
        d.mMessage.getData().asLongBuffer().get(result);
        return result;
    }

    /**
     * Deserializes an array of doubles at the given offset.
     */
    public double[] readDoubles(int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(8, expectedLength);
        double[] result = new double[si.numFields];
        d.mMessage.getData().position(d.mBaseOffset + DataHeader.HEADER_SIZE);
        d.mMessage.getData().asDoubleBuffer().get(result);
        return result;
    }

    /**
     * Deserializes an |Handle| at the given offset.
     */
    public Handle readHandle(int offset, boolean nullable) {
        int index = readInt(offset);
        if (index == -1) {
            if (!nullable) {
                throw new DeserializationException(
                        "Trying to decode an invalid handle for a non-nullable type.");
            }
            return InvalidHandle.INSTANCE;
        }
        mValidator.claimHandle(index);
        return mMessage.getHandles().get(index);
    }

    /**
     * Deserializes an |UntypedHandle| at the given offset.
     */
    public UntypedHandle readUntypedHandle(int offset, boolean nullable) {
        return readHandle(offset, nullable).toUntypedHandle();
    }

    /**
     * Deserializes a |ConsumerHandle| at the given offset.
     */
    public DataPipe.ConsumerHandle readConsumerHandle(int offset, boolean nullable) {
        return readUntypedHandle(offset, nullable).toDataPipeConsumerHandle();
    }

    /**
     * Deserializes a |ProducerHandle| at the given offset.
     */
    public DataPipe.ProducerHandle readProducerHandle(int offset, boolean nullable) {
        return readUntypedHandle(offset, nullable).toDataPipeProducerHandle();
    }

    /**
     * Deserializes a |MessagePipeHandle| at the given offset.
     */
    public MessagePipeHandle readMessagePipeHandle(int offset, boolean nullable) {
        return readUntypedHandle(offset, nullable).toMessagePipeHandle();
    }

    /**
     * Deserializes a |SharedBufferHandle| at the given offset.
     */
    public SharedBufferHandle readSharedBufferHandle(int offset, boolean nullable) {
        return readUntypedHandle(offset, nullable).toSharedBufferHandle();
    }

    /**
     * Deserializes an interface at the given offset.
     *
     * @return a proxy to the service.
     */
    public <P extends Proxy> P readServiceInterface(int offset, boolean nullable,
            Interface.Manager<?, P> manager) {
        MessagePipeHandle handle = readMessagePipeHandle(offset, nullable);
        if (!handle.isValid()) {
            return null;
        }
        return manager.attachProxy(handle);
    }

    /**
     * Deserializes a |InterfaceRequest| at the given offset.
     */
    public <I extends Interface> InterfaceRequest<I> readInterfaceRequest(int offset,
            boolean nullable) {
        MessagePipeHandle handle = readMessagePipeHandle(offset, nullable);
        if (handle == null) {
            return null;
        }
        return new InterfaceRequest<I>(handle);
    }

    /**
     * Deserializes a string at the given offset.
     */
    public String readString(int offset, boolean nullable) {
        final int arrayNullability = nullable ? BindingsHelper.ARRAY_NULLABLE : 0;
        byte[] bytes = readBytes(offset, arrayNullability, BindingsHelper.UNSPECIFIED_ARRAY_LENGTH);
        if (bytes == null) {
            return null;
        }
        return new String(bytes, Charset.forName("utf8"));
    }

    /**
     * Deserializes an array of |Handle| at the given offset.
     */
    public Handle[] readHandles(int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(4, expectedLength);
        Handle[] result = new Handle[si.numFields];
        for (int i = 0; i < result.length; ++i) {
            result[i] = d.readHandle(
                    DataHeader.HEADER_SIZE + BindingsHelper.SERIALIZED_HANDLE_SIZE * i,
                    BindingsHelper.isElementNullable(arrayNullability));
        }
        return result;
    }

    /**
     * Deserializes an array of |UntypedHandle| at the given offset.
     */
    public UntypedHandle[] readUntypedHandles(
            int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(4, expectedLength);
        UntypedHandle[] result = new UntypedHandle[si.numFields];
        for (int i = 0; i < result.length; ++i) {
            result[i] = d.readUntypedHandle(
                    DataHeader.HEADER_SIZE + BindingsHelper.SERIALIZED_HANDLE_SIZE * i,
                    BindingsHelper.isElementNullable(arrayNullability));
        }
        return result;
    }

    /**
     * Deserializes an array of |ConsumerHandle| at the given offset.
     */
    public DataPipe.ConsumerHandle[] readConsumerHandles(
            int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(4, expectedLength);
        DataPipe.ConsumerHandle[] result = new DataPipe.ConsumerHandle[si.numFields];
        for (int i = 0; i < result.length; ++i) {
            result[i] = d.readConsumerHandle(
                    DataHeader.HEADER_SIZE + BindingsHelper.SERIALIZED_HANDLE_SIZE * i,
                    BindingsHelper.isElementNullable(arrayNullability));
        }
        return result;
    }

    /**
     * Deserializes an array of |ProducerHandle| at the given offset.
     */
    public DataPipe.ProducerHandle[] readProducerHandles(
            int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(4, expectedLength);
        DataPipe.ProducerHandle[] result = new DataPipe.ProducerHandle[si.numFields];
        for (int i = 0; i < result.length; ++i) {
            result[i] = d.readProducerHandle(
                    DataHeader.HEADER_SIZE + BindingsHelper.SERIALIZED_HANDLE_SIZE * i,
                    BindingsHelper.isElementNullable(arrayNullability));
        }
        return result;

    }

    /**
     * Deserializes an array of |MessagePipeHandle| at the given offset.
     */
    public MessagePipeHandle[] readMessagePipeHandles(
            int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(4, expectedLength);
        MessagePipeHandle[] result = new MessagePipeHandle[si.numFields];
        for (int i = 0; i < result.length; ++i) {
            result[i] = d.readMessagePipeHandle(
                    DataHeader.HEADER_SIZE + BindingsHelper.SERIALIZED_HANDLE_SIZE * i,
                    BindingsHelper.isElementNullable(arrayNullability));
        }
        return result;

    }

    /**
     * Deserializes an array of |SharedBufferHandle| at the given offset.
     */
    public SharedBufferHandle[] readSharedBufferHandles(
            int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(4, expectedLength);
        SharedBufferHandle[] result = new SharedBufferHandle[si.numFields];
        for (int i = 0; i < result.length; ++i) {
            result[i] = d.readSharedBufferHandle(
                    DataHeader.HEADER_SIZE + BindingsHelper.SERIALIZED_HANDLE_SIZE * i,
                    BindingsHelper.isElementNullable(arrayNullability));
        }
        return result;

    }

    /**
     * Deserializes an array of |ServiceHandle| at the given offset.
     */
    public <S extends Interface, P extends Proxy> S[] readServiceInterfaces(
            int offset, int arrayNullability, int expectedLength, Interface.Manager<S, P> manager) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(4, expectedLength);
        S[] result = manager.buildArray(si.numFields);
        for (int i = 0; i < result.length; ++i) {
            // This cast is necessary because java 6 doesn't handle wildcard correctly when using
            // Manager<S, ? extends S>
            @SuppressWarnings("unchecked")
            S value = (S) d.readServiceInterface(
                    DataHeader.HEADER_SIZE + BindingsHelper.SERIALIZED_HANDLE_SIZE * i,
                    BindingsHelper.isElementNullable(arrayNullability), manager);
            result[i] = value;
        }
        return result;
    }

    /**
     * Deserializes an array of |InterfaceRequest| at the given offset.
     */
    public <I extends Interface> InterfaceRequest<I>[] readInterfaceRequests(
            int offset, int arrayNullability, int expectedLength) {
        Decoder d = readPointer(offset, BindingsHelper.isArrayNullable(arrayNullability));
        if (d == null) {
            return null;
        }
        DataHeader si = d.readDataHeaderForArray(4, expectedLength);
        @SuppressWarnings("unchecked")
        InterfaceRequest<I>[] result = new InterfaceRequest[si.numFields];
        for (int i = 0; i < result.length; ++i) {
            result[i] = d.readInterfaceRequest(
                    DataHeader.HEADER_SIZE + BindingsHelper.SERIALIZED_HANDLE_SIZE * i,
                    BindingsHelper.isElementNullable(arrayNullability));
        }
        return result;
    }

    /**
     * Returns a view of this decoder at the offset |offset|.
     */
    private Decoder getDecoderAtPosition(int offset) {
        return new Decoder(mMessage, mValidator, offset);
    }

    /**
     * Deserializes a {@link DataHeader} at the given offset and checks if it is correct for an
     * array of booleans.
     */
    private DataHeader readDataHeaderForBooleanArray(int expectedLength) {
        DataHeader dataHeader = readDataHeader();
        if (dataHeader.size < DataHeader.HEADER_SIZE + (dataHeader.numFields + 7) / 8) {
            throw new DeserializationException("Array header is incorrect.");
        }
        if (expectedLength != BindingsHelper.UNSPECIFIED_ARRAY_LENGTH
                && dataHeader.numFields != expectedLength) {
            throw new DeserializationException("Incorrect array length. Expected: " +
                    expectedLength + ", but got: " + dataHeader.numFields + ".");
        }
        return dataHeader;
    }

    /**
     * Deserializes a {@link DataHeader} of an array at the given offset.
     */
    private DataHeader readDataHeaderForArray(long elementSize, int expectedLength) {
        DataHeader dataHeader = readDataHeader();
        if (dataHeader.size < (DataHeader.HEADER_SIZE + elementSize * dataHeader.numFields)) {
            throw new DeserializationException("Array header is incorrect.");
        }
        if (expectedLength != BindingsHelper.UNSPECIFIED_ARRAY_LENGTH
                && dataHeader.numFields != expectedLength) {
            throw new DeserializationException("Incorrect array length. Expected: " +
                    expectedLength + ", but got: " + dataHeader.numFields + ".");
        }
        return dataHeader;
    }

    private void validateBufferSize(int offset, int size) {
        if (mMessage.getData().limit() < offset + size) {
            throw new DeserializationException("Buffer is smaller than expected.");
        }
    }
}
