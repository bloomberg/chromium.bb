// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import org.chromium.mojo.system.DataPipe.ProducerHandle;
import org.chromium.mojo.system.DataPipe.WriteFlags;

import java.nio.ByteBuffer;

/**
 * Implementation of {@link ProducerHandle}.
 */
class DataPipeProducerHandleImpl extends HandleImpl implements ProducerHandle {

    /**
     * @see HandleImpl#HandleImpl(CoreImpl, int)
     */
    DataPipeProducerHandleImpl(CoreImpl core, int mojoHandle) {
        super(core, mojoHandle);
    }

    /**
     * @see HandleImpl#HandleImpl(UntypedHandleImpl)
     */
    DataPipeProducerHandleImpl(UntypedHandleImpl handle) {
        super(handle);
    }

    /**
     * @see DataPipe.ProducerHandle#writeData(ByteBuffer, WriteFlags)
     */
    @Override
    public int writeData(ByteBuffer elements, WriteFlags flags) {
        return mCore.writeData(this, elements, flags);
    }

    /**
     * @see DataPipe.ProducerHandle#beginWriteData(int, WriteFlags)
     */
    @Override
    public ByteBuffer beginWriteData(int numBytes, WriteFlags flags) {
        return mCore.beginWriteData(this, numBytes, flags);
    }

    /**
     * @see DataPipe.ProducerHandle#endWriteData(int)
     */
    @Override
    public void endWriteData(int numBytesWritten) {
        mCore.endWriteData(this, numBytesWritten);
    }

}
