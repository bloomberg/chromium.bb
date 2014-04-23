// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import org.chromium.mojo.system.DataPipe.ConsumerHandle;
import org.chromium.mojo.system.DataPipe.ProducerHandle;

/**
 * Implementation of {@link UntypedHandle}.
 */
class UntypedHandleImpl extends HandleImpl implements UntypedHandle {

    /**
     * @see HandleImpl#HandleImpl(CoreImpl, int)
     */
    UntypedHandleImpl(CoreImpl core, int mojoHandle) {
        super(core, mojoHandle);
    }

    /**
     * @see org.chromium.mojo.system.UntypedHandle#toMessagePipeHandle()
     */
    @Override
    public MessagePipeHandle toMessagePipeHandle() {
        return new MessagePipeHandleImpl(this);
    }

    /**
     * @see org.chromium.mojo.system.UntypedHandle#toDataPipeConsumerHandle()
     */
    @Override
    public ConsumerHandle toDataPipeConsumerHandle() {
        return new DataPipeConsumerHandleImpl(this);
    }

    /**
     * @see org.chromium.mojo.system.UntypedHandle#toDataPipeProducerHandle()
     */
    @Override
    public ProducerHandle toDataPipeProducerHandle() {
        return new DataPipeProducerHandleImpl(this);
    }

    /**
     * @see org.chromium.mojo.system.UntypedHandle#toSharedBufferHandle()
     */
    @Override
    public SharedBufferHandle toSharedBufferHandle() {
        return new SharedBufferHandleImpl(this);
    }

}
