// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.graphics.ImageFormat;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * This class extends the VideoCaptureCamera base class for manipulating normal
 * video capture devices in Android, including receiving copies of preview
 * frames via Java-allocated buffers. It also includes class BuggyDeviceHack to
 * deal with troublesome devices.
 **/
@SuppressWarnings("deprecation")
public class VideoCaptureAndroid extends VideoCaptureCamera {

    // Some devices don't support YV12 format correctly, even with JELLY_BEAN or
    // newer OS. To work around the issues on those devices, we have to request
    // NV21. This is supposed to be a temporary hack.
    private static class BuggyDeviceHack {
        private static final String[] COLORSPACE_BUGGY_DEVICE_LIST = {
            "SAMSUNG-SGH-I747",
            "ODROID-U2",
            // See https://crbug.com/577435 for more info.
            "XT1092",
            "XT1095",
            "XT1096",
        };

        static int getImageFormat() {
            for (String buggyDevice : COLORSPACE_BUGGY_DEVICE_LIST) {
                if (buggyDevice.contentEquals(android.os.Build.MODEL)) {
                    return ImageFormat.NV21;
                }
            }
            return ImageFormat.YV12;
        }
    }

    private int mExpectedFrameSize;
    private static final int NUM_CAPTURE_BUFFERS = 3;
    private static final String TAG = "cr.media";

    static int getNumberOfCameras() {
        return android.hardware.Camera.getNumberOfCameras();
    }

    static int getCaptureApiType(int id) {
        if (VideoCaptureCamera.getCameraInfo(id) == null) {
            return CaptureApiType.API_TYPE_UNKNOWN;
        }
        return CaptureApiType.API1;
    }

    static String getName(int id) {
        android.hardware.Camera.CameraInfo cameraInfo = VideoCaptureCamera.getCameraInfo(id);
        if (cameraInfo == null) return null;

        return "camera " + id + ", facing " + (cameraInfo.facing
                == android.hardware.Camera.CameraInfo.CAMERA_FACING_FRONT ? "front" : "back");
    }

    static VideoCaptureFormat[] getDeviceSupportedFormats(int id) {
        android.hardware.Camera camera;
        try {
            camera = android.hardware.Camera.open(id);
        } catch (RuntimeException ex) {
            Log.e(TAG, "Camera.open: ", ex);
            return null;
        }
        android.hardware.Camera.Parameters parameters = getCameraParameters(camera);
        if (parameters == null) {
            return null;
        }

        ArrayList<VideoCaptureFormat> formatList = new ArrayList<VideoCaptureFormat>();
        // getSupportedPreview{Formats,FpsRange,PreviewSizes}() returns Lists
        // with at least one element, but when the camera is in bad state, they
        // can return null pointers; in that case we use a 0 entry, so we can
        // retrieve as much information as possible.
        List<Integer> pixelFormats = parameters.getSupportedPreviewFormats();
        if (pixelFormats == null) {
            pixelFormats = new ArrayList<Integer>();
        }
        if (pixelFormats.size() == 0) {
            pixelFormats.add(ImageFormat.UNKNOWN);
        }
        for (Integer previewFormat : pixelFormats) {
            int pixelFormat = AndroidImageFormat.UNKNOWN;
            if (previewFormat == ImageFormat.YV12) {
                pixelFormat = AndroidImageFormat.YV12;
            } else if (previewFormat == ImageFormat.NV21) {
                continue;
            }

            List<int[]> listFpsRange = parameters.getSupportedPreviewFpsRange();
            if (listFpsRange == null) {
                listFpsRange = new ArrayList<int[]>();
            }
            if (listFpsRange.size() == 0) {
                listFpsRange.add(new int[] {0, 0});
            }
            for (int[] fpsRange : listFpsRange) {
                List<android.hardware.Camera.Size> supportedSizes =
                        parameters.getSupportedPreviewSizes();
                if (supportedSizes == null) {
                    supportedSizes = new ArrayList<android.hardware.Camera.Size>();
                }
                if (supportedSizes.size() == 0) {
                    supportedSizes.add(camera.new Size(0, 0));
                }
                for (android.hardware.Camera.Size size : supportedSizes) {
                    formatList.add(new VideoCaptureFormat(size.width,
                                                     size.height,
                                                     (fpsRange[1] + 999) / 1000,
                                                     pixelFormat));
                }
            }
        }
        camera.release();
        return formatList.toArray(new VideoCaptureFormat[formatList.size()]);
    }

    VideoCaptureAndroid(Context context,
                        int id,
                        long nativeVideoCaptureDeviceAndroid) {
        super(context, id, nativeVideoCaptureDeviceAndroid);
    }

    @Override
    protected void setCaptureParameters(
            int width,
            int height,
            int frameRate,
            android.hardware.Camera.Parameters cameraParameters) {
        mCaptureFormat = new VideoCaptureFormat(
                width, height, frameRate, BuggyDeviceHack.getImageFormat());
    }

    @Override
    protected void allocateBuffers() {
        mExpectedFrameSize = mCaptureFormat.mWidth * mCaptureFormat.mHeight
                * ImageFormat.getBitsPerPixel(mCaptureFormat.mPixelFormat) / 8;
        for (int i = 0; i < NUM_CAPTURE_BUFFERS; i++) {
            byte[] buffer = new byte[mExpectedFrameSize];
            mCamera.addCallbackBuffer(buffer);
        }
    }

    @Override
    protected void setPreviewCallback(android.hardware.Camera.PreviewCallback cb) {
        mCamera.setPreviewCallbackWithBuffer(cb);
    }

    @Override
    public void onPreviewFrame(byte[] data, android.hardware.Camera camera) {
        mPreviewBufferLock.lock();
        try {
            if (!mIsRunning) {
                return;
            }
            if (data.length == mExpectedFrameSize) {
                nativeOnFrameAvailable(mNativeVideoCaptureDeviceAndroid,
                        data, mExpectedFrameSize, getCameraRotation());
            }
        } finally {
            mPreviewBufferLock.unlock();
            if (camera != null) {
                camera.addCallbackBuffer(data);
            }
        }
    }

    // TODO(wjia): investigate whether reading from texture could give better
    // performance and frame rate, using onFrameAvailable().
}
