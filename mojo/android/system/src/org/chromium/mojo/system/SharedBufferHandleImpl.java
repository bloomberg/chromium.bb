// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import java.nio.ByteBuffer;

/**
 * Implementation of {@link SharedBufferHandle}.
 */
class SharedBufferHandleImpl extends HandleImpl implements SharedBufferHandle {

    /**
     * @see HandleImpl#HandleImpl(CoreImpl, int)
     */
    SharedBufferHandleImpl(CoreImpl core, int mojoHandle) {
        super(core, mojoHandle);
    }

    /**
     * @see HandleImpl#HandleImpl(UntypedHandleImpl)
     */
    SharedBufferHandleImpl(UntypedHandleImpl handle) {
        super(handle);
    }

    /**
     * @see SharedBufferHandle#duplicate(DuplicateOptions)
     */
    @Override
    public SharedBufferHandle duplicate(DuplicateOptions options) {
        return mCore.duplicate(this, options);
    }

    /**
     * @see SharedBufferHandle#map(long, long, MapFlags)
     */
    @Override
    public ByteBuffer map(long offset, long numBytes, MapFlags flags) {
        return mCore.map(this, offset, numBytes, flags);
    }

    /**
     * @see SharedBufferHandle#unmap(ByteBuffer)
     */
    @Override
    public void unmap(ByteBuffer buffer) {
        mCore.unmap(buffer);
    }

}
