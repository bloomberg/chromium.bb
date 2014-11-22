// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.util.Log;
import android.util.Size;

import org.chromium.base.JNINamespace;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Static methods to retrieve information on current system cameras and their
 * capabilities using the Camera2 API introduced in Android SDK 21 (L Release).
 * For this we interact with an android.hardware.camera2.CameraManager.
 **/
@JNINamespace("media")
public class VideoCaptureCamera2 {
    private static final double kNanoSecondsToFps = 1.0E-9;
    private static final String TAG = "VideoCaptureCamera2";

    // Service function to grab CameraCharacteristics and handle exceptions.
    private static CameraCharacteristics getCameraCharacteristics(Context appContext, int id) {
        final CameraManager manager =
                (CameraManager) appContext.getSystemService(Context.CAMERA_SERVICE);
        try {
            return manager.getCameraCharacteristics(Integer.toString(id));
        } catch (CameraAccessException ex) {
            Log.e(TAG, "getNumberOfCameras: getCameraIdList(): " + ex);
        }
        return null;
    }

    static int getNumberOfCameras(Context appContext) {
        final CameraManager manager =
                (CameraManager) appContext.getSystemService(Context.CAMERA_SERVICE);
        try {
            return manager.getCameraIdList().length;
        } catch (CameraAccessException ex) {
            Log.e(TAG, "getNumberOfCameras: getCameraIdList(): " + ex);
            return 0;
        }
    }

    static String getName(int id, Context appContext) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(appContext, id);
        if (cameraCharacteristics == null) return null;
        int facing = cameraCharacteristics.get(CameraCharacteristics.LENS_FACING);

        return "camera2 " + id + ", facing "
                + ((facing == CameraCharacteristics.LENS_FACING_FRONT) ? "front" : "back");
    }

    static VideoCapture.CaptureFormat[] getDeviceSupportedFormats(Context appContext, int id) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(appContext, id);
        if (cameraCharacteristics == null) return null;

        final int[] capabilities = cameraCharacteristics.get(
                CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
        // Per-format frame rate via getOutputMinFrameDuration() is only available if the
        // property REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR is set.
        final boolean minFrameDurationAvailable = Arrays.asList(capabilities).contains(
                CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR);

        ArrayList<VideoCapture.CaptureFormat> formatList =
                new ArrayList<VideoCapture.CaptureFormat>();
        final StreamConfigurationMap streamMap =
                cameraCharacteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        final int[] formats = streamMap.getOutputFormats();
        for (int format : formats) {
            final Size[] sizes = streamMap.getOutputSizes(format);
            for (Size size : sizes) {
                double minFrameRate = 0.0f;
                if (minFrameDurationAvailable) {
                    final long minFrameDuration =
                            streamMap.getOutputMinFrameDuration(format, size);
                    minFrameRate = (minFrameDuration == 0) ? 0.0f :
                            (1.0 / kNanoSecondsToFps * minFrameDuration);
                } else {
                    // TODO(mcasas): find out where to get the info from in this case.
                    // Hint: perhaps using SCALER_AVAILABLE_PROCESSED_MIN_DURATIONS.
                    minFrameRate = 0.0;
                }
                formatList.add(new VideoCapture.CaptureFormat(size.getWidth(),
                                                              size.getHeight(),
                                                              (int) minFrameRate,
                                                              0));
            }
        }
        return formatList.toArray(new VideoCapture.CaptureFormat[formatList.size()]);
    }

    VideoCaptureCamera2() {}
}
