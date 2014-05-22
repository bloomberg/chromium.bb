// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.util.SparseArray;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;

/**
 * A class for reading back content.
 */
@JNINamespace("content")
public abstract class ContentReadbackHandler {
    /**
     * A callback interface for content readback into a bitmap.
     */
    public static interface GetContentBitmapCallback {
        /**
         * Called when the content readback finishes.
         * @param success    Indicates whether the readback succeeded or not.
         * @param bitmap     The {@link Bitmap} of the content.
         */
        public void onFinishGetContentBitmap(boolean success, Bitmap bitmap);
    }

    private int mNextReadbackId = 1;
    private SparseArray<GetContentBitmapCallback> mGetContentBitmapRequests;

    private long mNativeContentReadbackHandler;

    /**
     * Creates a {@link ContentReadbackHandler}.
     */
    public ContentReadbackHandler() {
        mGetContentBitmapRequests = new SparseArray<GetContentBitmapCallback>();
    }

    /**
     * Initialize the native object.
     */
    public void initNativeContentReadbackHandler() {
        mNativeContentReadbackHandler = nativeInit();
    }

    /**
     * Should be called when the ContentReadackHandler is not needed anymore.
     */
    public void destroy() {
        if (mNativeContentReadbackHandler != 0) nativeDestroy(mNativeContentReadbackHandler);
        mNativeContentReadbackHandler = 0;
    }


    @CalledByNative
    private void notifyGetContentBitmapFinished(int readbackId, boolean success, Bitmap bitmap) {
        GetContentBitmapCallback callback = mGetContentBitmapRequests.get(readbackId);
        if (callback != null) {
            mGetContentBitmapRequests.delete(readbackId);
            callback.onFinishGetContentBitmap(success, bitmap);
        } else {
            // readback Id is unregistered.
            assert false : "Readback finished for unregistered Id: " + readbackId;
        }
    }

    /**
     * Asynchronously, generate and grab a bitmap representing what is currently on the screen
     * for {@code view}.
     *
     * @param scale The scale that should be applied to the content.
     * @param srcRect  A subrect of the original content to capture. If this is empty, it will grab
     *                 the whole surface.
     * @param view  The {@link ContentViewCore} to grab the bitmap from.
     * @param callback The callback to be executed after readback completes.
     */
    public void getContentBitmapAsync(float scale, Rect srcRect, ContentViewCore view,
            GetContentBitmapCallback callback) {
        if (!readyForReadback()) {
            callback.onFinishGetContentBitmap(false, null);
            return;
        }
        ThreadUtils.assertOnUiThread();

        int readbackId = mNextReadbackId++;
        mGetContentBitmapRequests.put(readbackId, callback);
        nativeGetContentBitmap(mNativeContentReadbackHandler, readbackId, scale,
                Bitmap.Config.ARGB_8888, srcRect.top, srcRect.left, srcRect.width(),
                srcRect.height(), view);
    }

    /**
     * Implemented by the owner of this class to signal whether readback is possible or not.
     * @return Whether readback is possible or not.
     */
    protected abstract boolean readyForReadback();

    private native long nativeInit();
    private native void nativeDestroy(long nativeContentReadbackHandler);
    private native void nativeGetContentBitmap(long nativeContentReadbackHandler, int readback_id,
            float scale, Bitmap.Config config, float x, float y, float width, float height,
            Object contentViewCore);
}
