// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import android.util.Log;

import org.chromium.mojo.system.Core.WaitFlags;

/**
 * Implementation of {@link Handle}.
 */
class HandleImpl implements Handle {

    private static final String TAG = "HandleImpl";

    /**
     * The pointer to the scoped handle owned by this object.
     */
    private int mMojoHandle;

    /**
     * the core implementation. Will be used to delegate all behavior.
     */
    protected CoreImpl mCore;

    /**
     * Base constructor. Takes ownership of the passed handle.
     */
    HandleImpl(CoreImpl core, int mojoHandle) {
        mCore = core;
        mMojoHandle = mojoHandle;
    }

    /**
     * Constructor for transforming an {@link UntypedHandle} into a specific one.
     */
    HandleImpl(UntypedHandleImpl other) {
        mCore = other.mCore;
        HandleImpl otherAsHandleImpl = other;
        int mojoHandle = otherAsHandleImpl.mMojoHandle;
        otherAsHandleImpl.mMojoHandle = CoreImpl.INVALID_HANDLE;
        mMojoHandle = mojoHandle;
    }

    /**
     * @see org.chromium.mojo.system.Handle#close()
     */
    @Override
    public void close() {
        if (mMojoHandle != CoreImpl.INVALID_HANDLE) {
            // After a close, the handle is invalid whether the close succeed or not.
            int handle = mMojoHandle;
            mMojoHandle = CoreImpl.INVALID_HANDLE;
            mCore.close(handle);
        }
    }

    /**
     * @see org.chromium.mojo.system.Handle#wait(WaitFlags, long)
     */
    @Override
    public int wait(WaitFlags flags, long deadline) {
        return mCore.wait(this, flags, deadline);
    }

    /**
     * @see org.chromium.mojo.system.Handle#isValid()
     */
    @Override
    public boolean isValid() {
        return mMojoHandle != CoreImpl.INVALID_HANDLE;
    }

    /**
     * Getter for the native scoped handle.
     *
     * @return the native scoped handle.
     */
    int getMojoHandle() {
        return mMojoHandle;
    }

    /**
     * invalidate the handle. The caller must ensures that the handle does not leak.
     */
    void invalidateHandle() {
        mMojoHandle = CoreImpl.INVALID_HANDLE;
    }

    /**
     * Close the handle if it is valid. Necessary because we cannot let handle leak, and we cannot
     * ensure that every handle will be manually closed.
     *
     * @see java.lang.Object#finalize()
     */
    @Override
    protected final void finalize() throws Throwable {
        if (isValid()) {
            // This should not happen, as the user of this class should close the handle. Adding a
            // warning.
            Log.w(TAG, "Handle was not closed.");
            // Ignore result at this point.
            mCore.closeWithResult(mMojoHandle);
        }
        super.finalize();
    }

}
