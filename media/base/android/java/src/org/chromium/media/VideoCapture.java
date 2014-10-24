// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.graphics.ImageFormat;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * Video Capture Device base class, defines a set of methods that native code
 * needs to use to configure, start capture, and to be reached by callbacks and
 * provides some neccesary data type(s) with accessors.
 **/
@JNINamespace("media")
public abstract class VideoCapture {

    protected static class CaptureFormat {
        int mWidth;
        int mHeight;
        final int mFramerate;
        final int mPixelFormat;

        public CaptureFormat(
                int width, int height, int framerate, int pixelformat) {
            mWidth = width;
            mHeight = height;
            mFramerate = framerate;
            mPixelFormat = pixelformat;
        }

        public int getWidth() {
            return mWidth;
        }

        public int getHeight() {
            return mHeight;
        }

        public int getFramerate() {
            return mFramerate;
        }

        public int getPixelFormat() {
            return mPixelFormat;
        }
    }

    protected CaptureFormat mCaptureFormat = null;
    protected final Context mContext;
    protected final int mId;
    // Native callback context variable.
    protected final long mNativeVideoCaptureDeviceAndroid;

    VideoCapture(Context context,
                 int id,
                 long nativeVideoCaptureDeviceAndroid) {
        mContext = context;
        mId = id;
        mNativeVideoCaptureDeviceAndroid = nativeVideoCaptureDeviceAndroid;
    }

    @CalledByNative
    abstract boolean allocate(int width, int height, int frameRate);

    @CalledByNative
    abstract int startCapture();

    @CalledByNative
    abstract int stopCapture();

    @CalledByNative
    abstract void deallocate();

    // Local hook to allow derived classes to configure and plug capture
    // buffers if needed.
    abstract void allocateBuffers();

    @CalledByNative
    public final int queryWidth() {
        return mCaptureFormat.mWidth;
    }

    @CalledByNative
    public final int queryHeight() {
        return mCaptureFormat.mHeight;
    }

    @CalledByNative
    public final int queryFrameRate() {
        return mCaptureFormat.mFramerate;
    }

    @CalledByNative
    public final int getColorspace() {
        switch (mCaptureFormat.mPixelFormat) {
            case ImageFormat.YV12:
                return AndroidImageFormat.YV12;
            case ImageFormat.NV21:
                return AndroidImageFormat.NV21;
            case ImageFormat.UNKNOWN:
            default:
                return AndroidImageFormat.UNKNOWN;
        }
    }

    // Method for VideoCapture implementations to call back native code.
    public native void nativeOnFrameAvailable(
            long nativeVideoCaptureDeviceAndroid,
            byte[] data,
            int length,
            int rotation);
}
