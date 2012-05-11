// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.os.Looper;

import org.chromium.base.CalledByNative;
import org.chromium.base.WeakContext;

import java.util.List;

/**
 * Android implementation of the DeviceOrientation API.
 */
class DeviceOrientation implements SensorEventListener {

    // These fields are lazily initialized by getHandler().
    private Thread mThread;
    private Handler mHandler;

    // Non-zero if and only if we're listening for events.
    // To avoid race conditions on the C++ side, access must be synchronized.
    private int mNativePtr;

    // The gravity vector expressed in the body frame.
    private float[] mGravityVector;

    // The geomagnetic vector expressed in the body frame.
    private float[] mMagneticFieldVector;

    // Lazily initialized when registering for notifications.
    private SensorManager mSensorManager;

    private DeviceOrientation() {
    }

    /**
     * Start listening for sensor events. If this object is already listening
     * for events, the old callback is unregistered first.
     *
     * @param nativePtr Value to pass to nativeGotOrientation() for each event.
     * @param rateInMilliseconds Requested callback rate in milliseconds. The
     *            actual rate may be higher. Unwanted events should be ignored.
     * @return True on success.
     */
    @CalledByNative
    public synchronized boolean start(int nativePtr, int rateInMilliseconds) {
        stop();
        if (registerForSensors(rateInMilliseconds)) {
            mNativePtr = nativePtr;
            return true;
        }
        return false;
    }

    /**
     * Stop listening for sensor events. Always succeeds.
     *
     * We strictly guarantee that nativeGotOrientation() will not be called
     * after this method returns.
     */
    @CalledByNative
    public synchronized void stop() {
        if (mNativePtr != 0) {
            mNativePtr = 0;
            unregisterForSensors();
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // Nothing
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        switch (event.sensor.getType()) {
            case Sensor.TYPE_ACCELEROMETER:
                if (mGravityVector == null) {
                    mGravityVector = new float[3];
                }
                System.arraycopy(event.values, 0, mGravityVector, 0, mGravityVector.length);
                break;

            case Sensor.TYPE_MAGNETIC_FIELD:
                if (mMagneticFieldVector == null) {
                    mMagneticFieldVector = new float[3];
                }
                System.arraycopy(event.values, 0, mMagneticFieldVector, 0,
                                 mMagneticFieldVector.length);
                break;

            default:
                // Unexpected
                return;
        }

        getOrientationUsingGetRotationMatrix();
    }

    void getOrientationUsingGetRotationMatrix() {
        if (mGravityVector == null || mMagneticFieldVector == null) {
            return;
        }

        // Get the rotation matrix.
        // The rotation matrix that transforms from the body frame to the earth
        // frame.
        float[] deviceRotationMatrix = new float[9];
        if (!SensorManager.getRotationMatrix(deviceRotationMatrix, null, mGravityVector,
                mMagneticFieldVector)) {
            return;
        }

        // Convert rotation matrix to rotation angles.
        // Assuming that the rotations are appied in the order listed at
        // http://developer.android.com/reference/android/hardware/SensorEvent.html#values
        // the rotations are applied about the same axes and in the same order as required by the
        // API. The only conversions are sign changes as follows.  The angles are in radians

        float[] rotationAngles = new float[3];
        SensorManager.getOrientation(deviceRotationMatrix, rotationAngles);

        double alpha = Math.toDegrees(-rotationAngles[0]) - 90.0;
        while (alpha < 0.0) {
            alpha += 360.0; // [0, 360)
        }

        double beta = Math.toDegrees(-rotationAngles[1]);
        while (beta < -180.0) {
            beta += 360.0; // [-180, 180)
        }

        double gamma = Math.toDegrees(rotationAngles[2]);
        while (gamma < -90.0) {
            gamma += 360.0; // [-90, 90)
        }

        gotOrientation(alpha, beta, gamma);
    }

    boolean registerForSensors(int rateInMilliseconds) {
        if (registerForSensorType(Sensor.TYPE_ACCELEROMETER, rateInMilliseconds)
                && registerForSensorType(Sensor.TYPE_MAGNETIC_FIELD, rateInMilliseconds)) {
            return true;
        }
        unregisterForSensors();
        return false;
    }

    private SensorManager getSensorManager() {
        if (mSensorManager != null) {
            return mSensorManager;
        }
        mSensorManager = (SensorManager)WeakContext.getSystemService(Context.SENSOR_SERVICE);
        return mSensorManager;
    }

    void unregisterForSensors() {
        SensorManager sensorManager = getSensorManager();
        if (sensorManager == null) {
            return;
        }
        sensorManager.unregisterListener(this);
    }

    boolean registerForSensorType(int type, int rateInMilliseconds) {
        SensorManager sensorManager = getSensorManager();
        if (sensorManager == null) {
            return false;
        }
        List<Sensor> sensors = sensorManager.getSensorList(type);
        if (sensors.isEmpty()) {
            return false;
        }

        final int rateInMicroseconds = 1000 * rateInMilliseconds;
        // We want to err on the side of getting more events than we need.
        final int requestedRate = rateInMicroseconds / 2;

        // TODO(steveblock): Consider handling multiple sensors.
        return sensorManager.registerListener(this, sensors.get(0), requestedRate, getHandler());
    }

    synchronized void gotOrientation(double alpha, double beta, double gamma) {
        if (mNativePtr != 0) {
            nativeGotOrientation(mNativePtr, alpha, beta, gamma);
        }
    }

    private synchronized Handler getHandler() {
        // If we don't have a background thread, start it now.
        if (mThread == null) {
            mThread = new Thread(new Runnable() {
                public void run() {
                    Looper.prepare();
                    // Our Handler doesn't actually have to do anything, because
                    // SensorManager posts directly to the underlying Looper.
                    setHandler(new Handler());
                    Looper.loop();
                }
            });
            mThread.start();
        }
        // Wait for the background thread to spin up.
        while (mHandler == null) {
            try {
                wait();
            } catch (InterruptedException e) {
                // Somebody doesn't want us to wait! That's okay, SensorManager accepts null.
                return null;
            }
        }
        return mHandler;
    }

    private synchronized void setHandler(Handler handler) {
        mHandler = handler;
        notify();
    }

    @CalledByNative
    private static DeviceOrientation create() {
        return new DeviceOrientation();
    }

    /**
     * See chrome/browser/device_orientation/data_fetcher_impl_android.cc
     */
    private native void nativeGotOrientation(
            int nativePtr /* device_orientation::DataFetcherImplAndroid */,
            double alpha, double beta, double gamma);
}
