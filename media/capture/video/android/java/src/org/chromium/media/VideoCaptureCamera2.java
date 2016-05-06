// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Size;
import android.view.Surface;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/**
 * This class implements Video Capture using Camera2 API, introduced in Android
 * API 21 (L Release). Capture takes place in the current Looper, while pixel
 * download takes place in another thread used by ImageReader. A number of
 * static methods are provided to retrieve information on current system cameras
 * and their capabilities, using android.hardware.camera2.CameraManager.
 **/
@JNINamespace("media")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class VideoCaptureCamera2 extends VideoCapture {
    // Inner class to extend a CameraDevice state change listener.
    private class CrStateListener extends CameraDevice.StateCallback {
        @Override
        public void onOpened(CameraDevice cameraDevice) {
            mCameraDevice = cameraDevice;
            changeCameraStateAndNotify(CameraState.CONFIGURING);
            if (!createCaptureObjects()) {
                changeCameraStateAndNotify(CameraState.STOPPED);
                nativeOnError(mNativeVideoCaptureDeviceAndroid, "Error configuring camera");
            }
        }

        @Override
        public void onDisconnected(CameraDevice cameraDevice) {
            cameraDevice.close();
            mCameraDevice = null;
            changeCameraStateAndNotify(CameraState.STOPPED);
        }

        @Override
        public void onError(CameraDevice cameraDevice, int error) {
            cameraDevice.close();
            mCameraDevice = null;
            changeCameraStateAndNotify(CameraState.STOPPED);
            nativeOnError(mNativeVideoCaptureDeviceAndroid,
                    "Camera device error " + Integer.toString(error));
        }
    };

    // Inner class to extend a Capture Session state change listener.
    private class CrCaptureSessionListener extends CameraCaptureSession.StateCallback {
        @Override
        public void onConfigured(CameraCaptureSession cameraCaptureSession) {
            Log.d(TAG, "onConfigured");
            mCaptureSession = cameraCaptureSession;
            triggerCaptureSession();
            changeCameraStateAndNotify(CameraState.STARTED);
        }

        @Override
        public void onConfigureFailed(CameraCaptureSession cameraCaptureSession) {
            // TODO(mcasas): When signalling error, C++ will tear us down. Is there need for
            // cleanup?
            changeCameraStateAndNotify(CameraState.STOPPED);
            nativeOnError(mNativeVideoCaptureDeviceAndroid, "Camera session configuration error");
        }
    };

    // Internal class implementing the ImageReader listener. Gets pinged when a
    // new frame is been captured and downloaded to memory-backed buffers.
    private class CrImageReaderListener implements ImageReader.OnImageAvailableListener {
        @Override
        public void onImageAvailable(ImageReader reader) {
            try (Image image = reader.acquireLatestImage()) {
                if (image == null) return;

                if (image.getFormat() != ImageFormat.YUV_420_888 || image.getPlanes().length != 3) {
                    nativeOnError(mNativeVideoCaptureDeviceAndroid, "Unexpected image format: "
                            + image.getFormat() + " or #planes: " + image.getPlanes().length);
                    throw new IllegalStateException();
                }

                if (reader.getWidth() != image.getWidth()
                        || reader.getHeight() != image.getHeight()) {
                    nativeOnError(mNativeVideoCaptureDeviceAndroid, "ImageReader size ("
                            + reader.getWidth() + "x" + reader.getHeight()
                            + ") did not match Image size (" + image.getWidth() + "x"
                            + image.getHeight() + ")");
                    throw new IllegalStateException();
                }

                readImageIntoBuffer(image, mCapturedData);
                nativeOnFrameAvailable(mNativeVideoCaptureDeviceAndroid, mCapturedData,
                        mCapturedData.length, getCameraRotation());
            } catch (IllegalStateException ex) {
                Log.e(TAG, "acquireLatestImage():" + ex);
            }
        }
    };

    private byte[] mCapturedData;

    private CameraDevice mCameraDevice = null;
    private CaptureRequest.Builder mPreviewBuilder = null;
    private CameraCaptureSession mCaptureSession = null;
    private ImageReader mImageReader = null;

    private static final double kNanoSecondsToFps = 1.0E-9;
    private static final String TAG = "cr.media";

    private static enum CameraState { OPENING, CONFIGURING, STARTED, STOPPED }
    private CameraState mCameraState = CameraState.STOPPED;
    private final Object mCameraStateLock = new Object();

    // Service function to grab CameraCharacteristics and handle exceptions.
    private static CameraCharacteristics getCameraCharacteristics(Context appContext, int id) {
        final CameraManager manager =
                (CameraManager) appContext.getSystemService(Context.CAMERA_SERVICE);
        try {
            return manager.getCameraCharacteristics(Integer.toString(id));
        } catch (CameraAccessException ex) {
            Log.e(TAG, "getCameraCharacteristics: " + ex);
        }
        return null;
    }

    private boolean createCaptureObjects() {
        Log.d(TAG, "createCaptureObjects");
        if (mCameraDevice == null) return false;

        // Create an ImageReader and plug a thread looper into it to have
        // readback take place on its own thread.
        final int maxImages = 2;
        mImageReader = ImageReader.newInstance(mCaptureFormat.getWidth(),
                mCaptureFormat.getHeight(), mCaptureFormat.getPixelFormat(), maxImages);
        HandlerThread thread = new HandlerThread("CameraPreview");
        thread.start();
        final Handler backgroundHandler = new Handler(thread.getLooper());
        final CrImageReaderListener imageReaderListener = new CrImageReaderListener();
        mImageReader.setOnImageAvailableListener(imageReaderListener, backgroundHandler);

        // The Preview template specifically means "high frame rate is given
        // priority over the highest-quality post-processing".
        try {
            mPreviewBuilder = mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
        } catch (CameraAccessException | IllegalArgumentException | SecurityException ex) {
            Log.e(TAG, "createCaptureRequest: " + ex);
            return false;
        }
        if (mPreviewBuilder == null) {
            Log.e(TAG, "mPreviewBuilder error");
            return false;
        }
        // Construct an ImageReader Surface and plug it into our CaptureRequest.Builder.
        mPreviewBuilder.addTarget(mImageReader.getSurface());

        // A series of configuration options in the PreviewBuilder
        mPreviewBuilder.set(CaptureRequest.CONTROL_MODE, CameraMetadata.CONTROL_MODE_AUTO);
        mPreviewBuilder.set(
                CaptureRequest.NOISE_REDUCTION_MODE, CameraMetadata.NOISE_REDUCTION_MODE_FAST);
        mPreviewBuilder.set(CaptureRequest.EDGE_MODE, CameraMetadata.EDGE_MODE_FAST);
        mPreviewBuilder.set(CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE,
                CameraMetadata.CONTROL_VIDEO_STABILIZATION_MODE_ON);
        // SENSOR_EXPOSURE_TIME ?

        List<Surface> surfaceList = new ArrayList<Surface>(1);
        surfaceList.add(mImageReader.getSurface());
        final CrCaptureSessionListener captureSessionListener = new CrCaptureSessionListener();
        try {
            mCameraDevice.createCaptureSession(surfaceList, captureSessionListener, null);
        } catch (CameraAccessException | IllegalArgumentException | SecurityException ex) {
            Log.e(TAG, "createCaptureSession: " + ex);
            return false;
        }
        // Wait for trigger on CrCaptureSessionListener.onConfigured();
        return true;
    }

    private boolean triggerCaptureSession() {
        Log.d(TAG, "triggerCaptureSession");
        try {
            // This line triggers the capture. No |listener| is registered, so
            // we will not get notified of capture events, instead, ImageReader
            // will trigger every time a downloaded image is ready. Since
            //|handler| is null, we'll work on the current Thread Looper.
            mCaptureSession.setRepeatingRequest(mPreviewBuilder.build(), null, null);
        } catch (CameraAccessException | IllegalArgumentException | SecurityException ex) {
            Log.e(TAG, "setRepeatingRequest: " + ex);
            return false;
        }
        // Now wait for trigger on CrImageReaderListener.onImageAvailable();
        return true;
    }

    private static void readImageIntoBuffer(Image image, byte[] data) {
        final int imageWidth = image.getWidth();
        final int imageHeight = image.getHeight();
        final Image.Plane[] planes = image.getPlanes();

        int offset = 0;
        for (int plane = 0; plane < planes.length; ++plane) {
            final ByteBuffer buffer = planes[plane].getBuffer();
            final int rowStride = planes[plane].getRowStride();
            // Experimentally, U and V planes have |pixelStride| = 2, which
            // essentially means they are packed. That's silly, because we are
            // forced to unpack here.
            final int pixelStride = planes[plane].getPixelStride();
            final int planeWidth = (plane == 0) ? imageWidth : imageWidth / 2;
            final int planeHeight = (plane == 0) ? imageHeight : imageHeight / 2;

            if (pixelStride == 1 && rowStride == planeWidth) {
                // Copy whole plane from buffer into |data| at once.
                buffer.get(data, offset, planeWidth * planeHeight);
                offset += planeWidth * planeHeight;
            } else {
                // Copy pixels one by one respecting pixelStride and rowStride.
                byte[] rowData = new byte[rowStride];
                for (int row = 0; row < planeHeight - 1; ++row) {
                    buffer.get(rowData, 0, rowStride);
                    for (int col = 0; col < planeWidth; ++col) {
                        data[offset++] = rowData[col * pixelStride];
                    }
                }

                // Last row is special in some devices and may not contain the full
                // |rowStride| bytes of data. See http://crbug.com/458701.
                buffer.get(rowData, 0, Math.min(rowStride, buffer.remaining()));
                for (int col = 0; col < planeWidth; ++col) {
                    data[offset++] = rowData[col * pixelStride];
                }
            }
        }
    }

    private void changeCameraStateAndNotify(CameraState state) {
        synchronized (mCameraStateLock) {
            mCameraState = state;
            mCameraStateLock.notifyAll();
        }
    }

    static boolean isLegacyDevice(Context appContext, int id) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(appContext, id);
        return cameraCharacteristics != null
                && cameraCharacteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL)
                == CameraMetadata.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY;
    }

    static int getNumberOfCameras(Context appContext) {
        final CameraManager manager =
                (CameraManager) appContext.getSystemService(Context.CAMERA_SERVICE);
        try {
            return manager.getCameraIdList().length;
        } catch (CameraAccessException | SecurityException ex) {
            // SecurityException is an undocumented exception, but has been seen in
            // http://crbug/605424.
            Log.e(TAG, "getNumberOfCameras: getCameraIdList(): " + ex);
            return 0;
        }
    }

    static int getCaptureApiType(int id, Context appContext) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(appContext, id);
        if (cameraCharacteristics == null) {
            return CaptureApiType.API_TYPE_UNKNOWN;
        }

        final int supportedHWLevel =
                cameraCharacteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
        switch (supportedHWLevel) {
            case CameraMetadata.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY:
                return CaptureApiType.API2_LEGACY;
            case CameraMetadata.INFO_SUPPORTED_HARDWARE_LEVEL_FULL:
                return CaptureApiType.API2_FULL;
            case CameraMetadata.INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED:
                return CaptureApiType.API2_LIMITED;
            default:
                return CaptureApiType.API2_LEGACY;
        }
    }

    static String getName(int id, Context appContext) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(appContext, id);
        if (cameraCharacteristics == null) return null;
        final int facing = cameraCharacteristics.get(CameraCharacteristics.LENS_FACING);
        return "camera2 " + id + ", facing "
                + ((facing == CameraCharacteristics.LENS_FACING_FRONT) ? "front" : "back");
    }

    static VideoCaptureFormat[] getDeviceSupportedFormats(Context appContext, int id) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(appContext, id);
        if (cameraCharacteristics == null) return null;

        final int[] capabilities =
                cameraCharacteristics.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
        // Per-format frame rate via getOutputMinFrameDuration() is only available if the
        // property REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR is set.
        boolean minFrameDurationAvailable = false;
        for (int cap : capabilities) {
            if (cap == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR) {
                minFrameDurationAvailable = true;
                break;
            }
        }

        ArrayList<VideoCaptureFormat> formatList = new ArrayList<VideoCaptureFormat>();
        final StreamConfigurationMap streamMap =
                cameraCharacteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        final int[] formats = streamMap.getOutputFormats();
        for (int format : formats) {
            final Size[] sizes = streamMap.getOutputSizes(format);
            if (sizes == null) continue;
            for (Size size : sizes) {
                double minFrameRate = 0.0f;
                if (minFrameDurationAvailable) {
                    final long minFrameDuration = streamMap.getOutputMinFrameDuration(format, size);
                    minFrameRate = (minFrameDuration == 0)
                            ? 0.0f
                            : (1.0 / kNanoSecondsToFps * minFrameDuration);
                } else {
                    // TODO(mcasas): find out where to get the info from in this case.
                    // Hint: perhaps using SCALER_AVAILABLE_PROCESSED_MIN_DURATIONS.
                    minFrameRate = 0.0;
                }
                formatList.add(new VideoCaptureFormat(
                        size.getWidth(), size.getHeight(), (int) minFrameRate, 0));
            }
        }
        return formatList.toArray(new VideoCaptureFormat[formatList.size()]);
    }

    VideoCaptureCamera2(Context context, int id, long nativeVideoCaptureDeviceAndroid) {
        super(context, id, nativeVideoCaptureDeviceAndroid);
    }

    @Override
    public boolean allocate(int width, int height, int frameRate) {
        Log.d(TAG, "allocate: requested (%d x %d) @%dfps", width, height, frameRate);
        synchronized (mCameraStateLock) {
            if (mCameraState == CameraState.OPENING || mCameraState == CameraState.CONFIGURING) {
                Log.e(TAG, "allocate() invoked while Camera is busy opening/configuring.");
                return false;
            }
        }
        final CameraCharacteristics cameraCharacteristics = getCameraCharacteristics(mContext, mId);
        final StreamConfigurationMap streamMap =
                cameraCharacteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);

        // Find closest supported size.
        final Size[] supportedSizes = streamMap.getOutputSizes(ImageFormat.YUV_420_888);
        if (supportedSizes == null) return false;
        Size closestSupportedSize = null;
        int minDiff = Integer.MAX_VALUE;
        for (Size size : supportedSizes) {
            final int diff =
                    Math.abs(size.getWidth() - width) + Math.abs(size.getHeight() - height);
            if (diff < minDiff) {
                minDiff = diff;
                closestSupportedSize = size;
            }
        }
        if (minDiff == Integer.MAX_VALUE) {
            Log.e(TAG, "No supported resolutions.");
            return false;
        }
        Log.d(TAG, "allocate: matched (%d x %d)", closestSupportedSize.getWidth(),
                closestSupportedSize.getHeight());

        // |mCaptureFormat| is also used to configure the ImageReader.
        mCaptureFormat = new VideoCaptureFormat(closestSupportedSize.getWidth(),
                closestSupportedSize.getHeight(), frameRate, ImageFormat.YUV_420_888);
        int expectedFrameSize = mCaptureFormat.mWidth * mCaptureFormat.mHeight
                * ImageFormat.getBitsPerPixel(mCaptureFormat.mPixelFormat) / 8;
        mCapturedData = new byte[expectedFrameSize];
        mCameraNativeOrientation =
                cameraCharacteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);
        // TODO(mcasas): The following line is correct for N5 with prerelease Build,
        // but NOT for N7 with a dev Build. Figure out which one to support.
        mInvertDeviceOrientationReadings =
                cameraCharacteristics.get(CameraCharacteristics.LENS_FACING)
                == CameraCharacteristics.LENS_FACING_BACK;
        return true;
    }

    @Override
    public boolean startCapture() {
        Log.d(TAG, "startCapture");
        changeCameraStateAndNotify(CameraState.OPENING);
        final CameraManager manager =
                (CameraManager) mContext.getSystemService(Context.CAMERA_SERVICE);
        final Handler mainHandler = new Handler(mContext.getMainLooper());
        final CrStateListener stateListener = new CrStateListener();
        try {
            manager.openCamera(Integer.toString(mId), stateListener, mainHandler);
        } catch (CameraAccessException | IllegalArgumentException | SecurityException ex) {
            Log.e(TAG, "allocate: manager.openCamera: " + ex);
            return false;
        }

        return true;
    }

    @Override
    public boolean stopCapture() {
        Log.d(TAG, "stopCapture");

        // With Camera2 API, the capture is started asynchronously, which will cause problem if
        // stopCapture comes too quickly. Without stopping the previous capture properly, the next
        // startCapture will fail and make Chrome no-responding. So wait camera to be STARTED.
        synchronized (mCameraStateLock) {
            while (mCameraState != CameraState.STARTED && mCameraState != CameraState.STOPPED) {
                try {
                    mCameraStateLock.wait();
                } catch (InterruptedException ex) {
                    Log.e(TAG, "CaptureStartedEvent: " + ex);
                }
            }
            if (mCameraState == CameraState.STOPPED) return true;
        }

        try {
            mCaptureSession.abortCaptures();
        } catch (CameraAccessException | IllegalStateException ex) {
            Log.e(TAG, "abortCaptures: " + ex);
            return false;
        }
        if (mCameraDevice == null) return false;
        mCameraDevice.close();
        changeCameraStateAndNotify(CameraState.STOPPED);
        return true;
    }

    @Override
    public void deallocate() {
        Log.d(TAG, "deallocate");
    }
}
