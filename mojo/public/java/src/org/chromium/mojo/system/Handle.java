// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import java.io.Closeable;

/**
 * A generic mojo handle.
 */
public interface Handle extends Closeable {

    /**
     * Closes the given |handle|.
     * <p>
     * Concurrent operations on |handle| may succeed (or fail as usual) if they happen before the
     * close, be cancelled with result |MojoResult.CANCELLED| if they properly overlap (this is
     * likely the case with |wait()|, etc.), or fail with |MojoResult.INVALID_ARGUMENT| if they
     * happen after.
     */
    @Override
    public void close();

    /**
     * @see Core#wait(Handle, Core.WaitFlags, long)
     */
    public int wait(Core.WaitFlags flags, long deadline);

    /**
     * @return whether the handle is valid. A handle is valid until it has been explicitly closed or
     *         send through a message pipe via |MessagePipeHandle.writeMessage|.
     */
    public boolean isValid();

}
