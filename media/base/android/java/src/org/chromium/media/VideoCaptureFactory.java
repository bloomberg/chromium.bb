// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
// Needed for jni_generator.py to guess correctly the origin of
// VideoCapture.CaptureFormat.
import org.chromium.media.VideoCapture;

/**
 * This class implements a factory of Android Video Capture objects for Chrome.
 * The static createVideoCapture() returns either a "normal" VideoCaptureAndroid
 * or a "special" VideoCaptureTango. Cameras are identified by |id|, where Tango
 * cameras have |id| above the standard ones. Video Capture objects allocated
 * via createVideoCapture() are explicitly owned by the caller.
 * ChromiumCameraInfo is an internal class with some static methods needed from
 * the rest of the class to manipulate the |id|s of normal and special devices.
 **/
@JNINamespace("media")
@SuppressWarnings("deprecation")
class VideoCaptureFactory {

    // Internal class to encapsulate camera device id manipulations.
    static class ChromiumCameraInfo {
        // Special devices have more cameras than usual. Those devices are
        // identified by model & device. Currently only the Tango is supported.
        // Note that these devices have no Camera.CameraInfo.
        private static final String[][] SPECIAL_DEVICE_LIST = {
            {"Peanut", "peanut"},
        };
        private static int sNumberOfSystemCameras = -1;
        private static final String TAG = "ChromiumCameraInfo";

        private static boolean isSpecialDevice() {
            for (String[] device : SPECIAL_DEVICE_LIST) {
                if (device[0].contentEquals(android.os.Build.MODEL)
                        && device[1].contentEquals(android.os.Build.DEVICE)) {
                    return true;
                }
            }
            return false;
        }

        private static boolean isSpecialCamera(int id) {
            return id >= sNumberOfSystemCameras;
        }

        private static int toSpecialCameraId(int id) {
            assert isSpecialCamera(id);
            return id - sNumberOfSystemCameras;
        }

        private static int getNumberOfCameras(Context appContext) {
            // getNumberOfCameras() would not fail due to lack of permission, but the
            // following operations on camera would. "No permission" isn't a fatal
            // error in WebView, specially for those applications which have no purpose
            // to use a camera, but "load page" requires it. So, output a warning log
            // and carry on pretending the system has no camera(s).
            if (sNumberOfSystemCameras == -1) {
                if (PackageManager.PERMISSION_GRANTED
                        == appContext.getPackageManager().checkPermission(
                                "android.permission.CAMERA", appContext.getPackageName())) {
                    if (isLReleaseOrLater()) {
                        sNumberOfSystemCameras =
                                VideoCaptureCamera2.getNumberOfCameras(appContext);
                    } else {
                        sNumberOfSystemCameras = VideoCaptureAndroid.getNumberOfCameras();
                        if (isSpecialDevice()) {
                            Log.d(TAG, "Special device: " + android.os.Build.MODEL);
                            sNumberOfSystemCameras += VideoCaptureTango.numberOfCameras();
                        }
                    }
                } else {
                    sNumberOfSystemCameras = 0;
                    Log.w(TAG, "Missing android.permission.CAMERA permission, "
                                  + "no system camera available.");
                }
            }
            return sNumberOfSystemCameras;
        }
    }

    private static boolean isLReleaseOrLater() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
    }

    // Factory methods.
    @CalledByNative
    static VideoCapture createVideoCapture(
            Context context, int id, long nativeVideoCaptureDeviceAndroid) {
        if (isLReleaseOrLater() && !VideoCaptureCamera2.isLegacyDevice(context, id)) {
            return new VideoCaptureCamera2(context,
                                           id,
                                           nativeVideoCaptureDeviceAndroid);
        }
        if (!ChromiumCameraInfo.isSpecialCamera(id)) {
            return new VideoCaptureAndroid(context,
                                           id,
                                           nativeVideoCaptureDeviceAndroid);
        }
        return new VideoCaptureTango(context,
                                     ChromiumCameraInfo.toSpecialCameraId(id),
                                     nativeVideoCaptureDeviceAndroid);
    }

    @CalledByNative
    static int getNumberOfCameras(Context appContext) {
        return ChromiumCameraInfo.getNumberOfCameras(appContext);
    }

    @CalledByNative
    static String getDeviceName(int id, Context appContext) {
        if (isLReleaseOrLater() && !VideoCaptureCamera2.isLegacyDevice(appContext, id)) {
            return VideoCaptureCamera2.getName(id, appContext);
        }
        return (ChromiumCameraInfo.isSpecialCamera(id))
                ? VideoCaptureTango.getName(ChromiumCameraInfo.toSpecialCameraId(id))
                : VideoCaptureAndroid.getName(id);
    }

    @CalledByNative
    static VideoCapture.CaptureFormat[] getDeviceSupportedFormats(Context appContext, int id) {
        if (isLReleaseOrLater() && !VideoCaptureCamera2.isLegacyDevice(appContext, id)) {
            return VideoCaptureCamera2.getDeviceSupportedFormats(appContext, id);
        }
        return ChromiumCameraInfo.isSpecialCamera(id)
                ? VideoCaptureTango.getDeviceSupportedFormats(
                        ChromiumCameraInfo.toSpecialCameraId(id))
                : VideoCaptureAndroid.getDeviceSupportedFormats(id);
    }

    @CalledByNative
    static int getCaptureFormatWidth(VideoCapture.CaptureFormat format) {
        return format.getWidth();
    }

    @CalledByNative
    static int getCaptureFormatHeight(VideoCapture.CaptureFormat format) {
        return format.getHeight();
    }

    @CalledByNative
    static int getCaptureFormatFramerate(VideoCapture.CaptureFormat format) {
        return format.getFramerate();
    }

    @CalledByNative
    static int getCaptureFormatPixelFormat(VideoCapture.CaptureFormat format) {
        return format.getPixelFormat();
    }
}
