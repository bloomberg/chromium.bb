// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.content.Context;
import android.content.Intent;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Surface;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.nio.ByteBuffer;

/**
 * This class implements Screen Capture using projection API, introduced in Android
 * API 21 (L Release). Capture takes place in the current Looper, while pixel
 * download takes place in another thread used by ImageReader.
 **/
@JNINamespace("media")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class ScreenCapture extends Fragment {
    private static final String TAG = "ScreenCaptureMachine";

    private static final int REQUEST_MEDIA_PROJECTION = 1;

    // Native callback context variable.
    private final long mNativeScreenCaptureMachineAndroid;
    private final Context mContext;

    private static enum CaptureState { ATTACHED, ALLOWED, STARTED, STOPPING, STOPPED }
    private final Object mCaptureStateLock = new Object();
    private CaptureState mCaptureState = CaptureState.STOPPED;

    private MediaProjection mMediaProjection;
    private MediaProjectionManager mMediaProjectionManager;
    private VirtualDisplay mVirtualDisplay;
    private Surface mSurface;
    private ImageReader mImageReader = null;
    private HandlerThread mThread;
    private Handler mBackgroundHandler;

    private int mScreenDensity;
    private int mWidth;
    private int mHeight;
    private int mFormat;
    private int mResultCode;
    private Intent mResultData;

    ScreenCapture(Context context, long nativeScreenCaptureMachineAndroid) {
        mContext = context;
        mNativeScreenCaptureMachineAndroid = nativeScreenCaptureMachineAndroid;

        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) {
            Log.e(TAG, "activity is null");
            return;
        }

        FragmentManager fragmentManager = activity.getFragmentManager();
        FragmentTransaction fragmentTransaction = fragmentManager.beginTransaction();
        fragmentTransaction.add(this, "screencapture");

        try {
            fragmentTransaction.commit();
        } catch (RuntimeException e) {
            Log.e(TAG, "ScreenCaptureExcaption " + e);
        }

        DisplayMetrics metrics = new DisplayMetrics();
        Display display = activity.getWindowManager().getDefaultDisplay();
        display.getMetrics(metrics);
        mScreenDensity = metrics.densityDpi;
    }

    // Factory method.
    @CalledByNative
    static ScreenCapture createScreenCaptureMachine(
            Context context, long nativeScreenCaptureMachineAndroid) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return new ScreenCapture(context, nativeScreenCaptureMachineAndroid);
        }
        return null;
    }

    // Internal class implementing the ImageReader listener. Gets pinged when a
    // new frame is been captured and downloaded to memory-backed buffers.
    // TODO(braveyao): This is very similar as the one in VideoCaptureCamera2. Try to see
    // if we can extend from it, https://crbug.com/487935.
    private class CrImageReaderListener implements ImageReader.OnImageAvailableListener {
        @Override
        public void onImageAvailable(ImageReader reader) {
            synchronized (mCaptureStateLock) {
                if (mCaptureState != CaptureState.STARTED) {
                    Log.e(TAG, "Get captured frame in unexpected state.");
                    return;
                }
            }

            try (Image image = reader.acquireLatestImage()) {
                if (image == null) return;
                if (reader.getWidth() != image.getWidth()
                        || reader.getHeight() != image.getHeight()) {
                    Log.e(TAG, "ImageReader size (" + reader.getWidth() + "x" + reader.getHeight()
                                    + ") did not match Image size (" + image.getWidth() + "x"
                                    + image.getHeight() + ")");
                    throw new IllegalStateException();
                }

                switch (image.getFormat()) {
                    case PixelFormat.RGBA_8888:
                        if (image.getPlanes().length != 1) {
                            Log.e(TAG, "Unexpected image planes for RGBA_8888 format: "
                                            + image.getPlanes().length);
                            throw new IllegalStateException();
                        }

                        nativeOnRGBAFrameAvailable(mNativeScreenCaptureMachineAndroid,
                                image.getPlanes()[0].getBuffer(),
                                image.getPlanes()[0].getRowStride(), image.getCropRect().left,
                                image.getCropRect().top, image.getCropRect().width(),
                                image.getCropRect().height(), image.getTimestamp());
                        break;
                    case ImageFormat.YUV_420_888:
                        if (image.getPlanes().length != 3) {
                            Log.e(TAG, "Unexpected image planes for YUV_420_888 format: "
                                            + image.getPlanes().length);
                            throw new IllegalStateException();
                        }

                        // The pixel stride of Y plane is always 1. The U/V planes are guaranteed
                        // to have the same row stride and pixel stride.
                        nativeOnI420FrameAvailable(mNativeScreenCaptureMachineAndroid,
                                image.getPlanes()[0].getBuffer(),
                                image.getPlanes()[0].getRowStride(),
                                image.getPlanes()[1].getBuffer(), image.getPlanes()[2].getBuffer(),
                                image.getPlanes()[1].getRowStride(),
                                image.getPlanes()[1].getPixelStride(), image.getCropRect().left,
                                image.getCropRect().top, image.getCropRect().width(),
                                image.getCropRect().height(), image.getTimestamp());
                        break;
                    default:
                        Log.e(TAG, "Unexpected image format: " + image.getFormat());
                        throw new IllegalStateException();
                }
            } catch (IllegalStateException ex) {
                Log.e(TAG, "acquireLatestImage():" + ex);
            } catch (UnsupportedOperationException ex) {
                Log.i(TAG, "acquireLatestImage():" + ex);
                // YUV_420_888 is the preference, but not all devices support it,
                // fall-back to RGBA_8888 then.
                mImageReader.close();
                mVirtualDisplay.release();

                mFormat = PixelFormat.RGBA_8888;
                createImageReaderWithFormat(mFormat);
                createVirtualDisplay();
            }
        }
    }

    private class MediaProjectionCallback extends MediaProjection.Callback {
        @Override
        public void onStop() {
            changeCaptureStateAndNotify(CaptureState.STOPPED);
            mMediaProjection = null;
            if (mVirtualDisplay == null) return;
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        Log.d(TAG, "onAttach");
        changeCaptureStateAndNotify(CaptureState.ATTACHED);
    }

    // This method was deprecated in API level 23 by onAttach(Context).
    // TODO(braveyao): remove this method after the minSdkVersion of chrome is 23,
    // https://crbug.com/614172.
    @SuppressWarnings("deprecation")
    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        Log.d(TAG, "onAttach");
        changeCaptureStateAndNotify(CaptureState.ATTACHED);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        Log.d(TAG, "onDetach");
        stopCapture();
    }

    @CalledByNative
    public boolean startPrompt(int width, int height) {
        Log.d(TAG, "startPrompt");
        synchronized (mCaptureStateLock) {
            while (mCaptureState != CaptureState.ATTACHED) {
                try {
                    mCaptureStateLock.wait();
                } catch (InterruptedException ex) {
                    Log.e(TAG, "ScreenCaptureException: " + ex);
                }
            }
        }

        mWidth = width;
        mHeight = height;

        mMediaProjectionManager = (MediaProjectionManager) mContext.getSystemService(
                Context.MEDIA_PROJECTION_SERVICE);
        if (mMediaProjectionManager == null) {
            Log.e(TAG, "mMediaProjectionManager is null");
            return false;
        }

        try {
            startActivityForResult(
                    mMediaProjectionManager.createScreenCaptureIntent(), REQUEST_MEDIA_PROJECTION);
        } catch (android.content.ActivityNotFoundException e) {
            Log.e(TAG, "ScreenCaptureException " + e);
            return false;
        }
        return true;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != REQUEST_MEDIA_PROJECTION) return;

        if (resultCode == Activity.RESULT_OK) {
            mResultCode = resultCode;
            mResultData = data;
            changeCaptureStateAndNotify(CaptureState.ALLOWED);
        }
        nativeOnActivityResult(
                mNativeScreenCaptureMachineAndroid, resultCode == Activity.RESULT_OK);
    }

    @CalledByNative
    public void startCapture() {
        Log.d(TAG, "startCapture");
        synchronized (mCaptureStateLock) {
            if (mCaptureState != CaptureState.ALLOWED) {
                Log.e(TAG, "startCapture() invoked without user permission.");
                return;
            }
        }
        mMediaProjection = mMediaProjectionManager.getMediaProjection(mResultCode, mResultData);
        if (mMediaProjection == null) {
            Log.e(TAG, "mMediaProjection is null");
            return;
        }
        mMediaProjection.registerCallback(new MediaProjectionCallback(), null);

        mThread = new HandlerThread("ScreenCapture");
        mThread.start();
        mBackgroundHandler = new Handler(mThread.getLooper());

        // On Android M and above, YUV420 is prefered. Some Android L devices will silently
        // fail with YUV420, so keep with RGBA_8888 on L.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            mFormat = PixelFormat.RGBA_8888;
        } else {
            mFormat = ImageFormat.YUV_420_888;
        }
        createImageReaderWithFormat(mFormat);
        createVirtualDisplay();

        changeCaptureStateAndNotify(CaptureState.STARTED);
    }

    @CalledByNative
    public void stopCapture() {
        Log.d(TAG, "stopCapture");
        synchronized (mCaptureStateLock) {
            if (mMediaProjection != null && mCaptureState == CaptureState.STARTED) {
                mMediaProjection.stop();
                changeCaptureStateAndNotify(CaptureState.STOPPING);
            }

            while (mCaptureState != CaptureState.STOPPED) {
                try {
                    mCaptureStateLock.wait();
                } catch (InterruptedException ex) {
                    Log.e(TAG, "ScreenCaptureEvent: " + ex);
                }
            }
        }
    }

    private void createImageReaderWithFormat(int format) {
        final int maxImages = 2;
        mImageReader = ImageReader.newInstance(mWidth, mHeight, format, maxImages);
        mSurface = mImageReader.getSurface();
        final CrImageReaderListener imageReaderListener = new CrImageReaderListener();
        mImageReader.setOnImageAvailableListener(imageReaderListener, mBackgroundHandler);
    }

    private void createVirtualDisplay() {
        mVirtualDisplay = mMediaProjection.createVirtualDisplay("ScreenCapture", mWidth, mHeight,
                mScreenDensity, DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR, mSurface, null,
                null);
    }

    private void changeCaptureStateAndNotify(CaptureState state) {
        synchronized (mCaptureStateLock) {
            mCaptureState = state;
            mCaptureStateLock.notifyAll();
        }
    }

    // Method for ScreenCapture implementations to call back native code.
    private native void nativeOnRGBAFrameAvailable(long nativeScreenCaptureMachineAndroid,
            ByteBuffer buf, int left, int top, int width, int height, int rowStride,
            long timestamp);
    // Method for ScreenCapture implementations to call back native code.
    private native void nativeOnI420FrameAvailable(long nativeScreenCaptureMachineAndroid,
            ByteBuffer yBuffer, int yStride, ByteBuffer uBuffer, ByteBuffer vBuffer,
            int uvRowStride, int uvPixelStride, int left, int top, int width, int height,
            long timestamp);
    // Method for ScreenCapture implementations to call back native code.
    private native void nativeOnActivityResult(
            long nativeScreenCaptureMachineAndroid, boolean result);
}
