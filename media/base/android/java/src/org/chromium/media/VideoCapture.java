// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.graphics.ImageFormat;
import android.view.Surface;
import android.view.WindowManager;

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

    // The angle (0, 90, 180, 270) that the image needs to be rotated to show in
    // the display's native orientation.
    protected int mCameraNativeOrientation;
    // In some occasions we need to invert the device rotation readings, see the
    // individual implementations.
    protected boolean mInvertDeviceOrientationReadings;

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

    // Allocate necessary resources for capture.
    @CalledByNative
    public abstract boolean allocate(int width, int height, int frameRate);

    // Starts actual capture.
    @CalledByNative
    public abstract boolean startCapture();

    // Stops current capture.
    @CalledByNative
    public abstract boolean stopCapture();

    @CalledByNative
    public abstract void deallocate();

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
            case ImageFormat.YUV_420_888:
                return AndroidImageFormat.YUV_420_888;
            case ImageFormat.NV21:
                return AndroidImageFormat.NV21;
            case ImageFormat.UNKNOWN:
            default:
                return AndroidImageFormat.UNKNOWN;
        }
    }

    protected final int getCameraRotation() {
        int rotation = mInvertDeviceOrientationReadings
                ?  (360 - getDeviceRotation()) : getDeviceRotation();
        return (mCameraNativeOrientation + rotation) % 360;
    }

    protected final int getDeviceRotation() {
        if (mContext == null) return 0;
        final int orientation;
        WindowManager wm = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        switch(wm.getDefaultDisplay().getRotation()) {
            case Surface.ROTATION_90:
                orientation = 90;
                break;
            case Surface.ROTATION_180:
                orientation = 180;
                break;
            case Surface.ROTATION_270:
                orientation = 270;
                break;
            case Surface.ROTATION_0:
            default:
                orientation = 0;
                break;
        }
        return orientation;
    }

    // Method for VideoCapture implementations to call back native code.
    public native void nativeOnFrameAvailable(long nativeVideoCaptureDeviceAndroid,
                                              byte[] data,
                                              int length,
                                              int rotation);

    // Method for VideoCapture implementations to signal an asynchronous error.
    public native void nativeOnError(long nativeVideoCaptureDeviceAndroid,
                                     String message);
}
