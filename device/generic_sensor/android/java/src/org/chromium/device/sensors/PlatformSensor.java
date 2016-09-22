// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.sensors;

import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.os.Build;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.device.mojom.ReportingMode;

import java.nio.BufferOverflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import java.util.List;

/**
 * Implementation of PlatformSensor that uses Android Sensor Framework. Lifetime is controlled by
 * the device::PlatformSensorAndroid.
 */
@JNINamespace("device")
public class PlatformSensor implements SensorEventListener {
    private static final double MICROSECONDS_IN_SECOND = 1000000;
    private static final double MILLISECONDS_IN_NANOSECOND = 0.000001d;

    /**
     * The SENSOR_FREQUENCY_NORMAL is defined as 5Hz which corresponds to a polling delay
     * @see android.hardware.SensorManager.SENSOR_DELAY_NORMAL value that is defined as 200000
     * microseconds.
     */
    private static final double SENSOR_FREQUENCY_NORMAL = 5.0d;

    /**
     * Identifier of device::PlatformSensorAndroid instance.
     */
    private long mNativePlatformSensorAndroid;

    /**
     * Shared buffer that is used to return data to the client.
     */
    private ByteBuffer mBuffer;

    /**
     * Buffer that is filled with sensor reading data and swapped into mBuffer.
     */
    private ByteBuffer mSensorReadingData;

    /**
     * Used for fetching sensor reading values and obtaining information about the sensor.
     * @see android.hardware.Sensor
     */
    private final Sensor mSensor;

    /**
     * The minimum delay between two readings in microseconds that is supported by the sensor.
     */
    private final int mMinDelayUsec;

    /**
     * The number of sensor reading values required from the sensor.
     */
    private final int mReadingCount;

    /**
     * Frequncy that is currently used by the sensor for polling.
     */
    private double mCurrentPollingFrequency;

    /**
     * Provides shared SensorManager and event processing thread Handler to PlatformSensor objects.
     */
    private final PlatformSensorProvider mProvider;

    /**
     * Creates new PlatformSensor.
     *
     * @param sensorType type of the sensor to be constructed. @see android.hardware.Sensor.TYPE_*
     * @param readingCount number of sensor reading values required from the sensor.
     * @param provider object that shares SensorManager and polling thread Handler with sensors.
     */
    public static PlatformSensor create(
            int sensorType, int readingCount, PlatformSensorProvider provider) {
        List<Sensor> sensors = provider.getSensorManager().getSensorList(sensorType);
        if (sensors.isEmpty()) return null;
        return new PlatformSensor(sensors.get(0), readingCount, provider);
    }

    /**
     * Constructor.
     */
    protected PlatformSensor(Sensor sensor, int readingCount, PlatformSensorProvider provider) {
        mReadingCount = readingCount;
        mProvider = provider;
        mSensor = sensor;
        mMinDelayUsec = mSensor.getMinDelay();
    }

    /**
     * Initializes PlatformSensor, called by native code.
     *
     * @param nativePlatformSensorAndroid identifier of device::PlatformSensorAndroid instance.
     * @param buffer shared buffer that is used to return data to the client.
     */
    @CalledByNative
    protected void initPlatformSensorAndroid(long nativePlatformSensorAndroid, ByteBuffer buffer) {
        assert nativePlatformSensorAndroid != 0;
        assert buffer != null;
        mNativePlatformSensorAndroid = nativePlatformSensorAndroid;
        mBuffer = buffer;
        mSensorReadingData = ByteBuffer.allocate(mBuffer.capacity());
        mSensorReadingData.order(ByteOrder.nativeOrder());
    }

    /**
     * Fills shared buffer with sensor reading data, throws exception if number of received values
     * is less than the number required for the PlatformSensor.
     */
    private void fillSensorReadingData(SensorEvent event, ByteBuffer buffer)
            throws IllegalStateException {
        if (event.values.length < mReadingCount) throw new IllegalStateException();
        for (int i = 0; i < mReadingCount; ++i) {
            buffer.putDouble(event.values[i]);
        }
    }

    /**
     * Returns reporting mode supported by the sensor.
     *
     * @return ReportingMode reporting mode.
     */
    @CalledByNative
    protected int getReportingMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return mSensor.getReportingMode() == Sensor.REPORTING_MODE_CONTINUOUS
                    ? ReportingMode.CONTINUOUS
                    : ReportingMode.ON_CHANGE;
        }
        return ReportingMode.CONTINUOUS;
    }

    /**
     * Returns default configuration supported by the sensor. Currently only frequency is supported.
     *
     * @return double frequency.
     */
    @CalledByNative
    protected double getDefaultConfiguration() {
        return SENSOR_FREQUENCY_NORMAL;
    }

    /**
     * Requests sensor to start polling for data.
     *
     * @return boolean true if successful, false otherwise.
     */
    @CalledByNative
    protected boolean startSensor(double frequency) {
        // If we already polling hw with same frequency, do not restart the sensor.
        if (mCurrentPollingFrequency == frequency) return true;

        // Unregister old listener if polling frequency has changed.
        unregisterListener();
        mProvider.sensorStarted(this);
        boolean sensorStarted = mProvider.getSensorManager().registerListener(
                this, mSensor, getSamplingPeriod(frequency), mProvider.getHandler());

        if (!sensorStarted) {
            stopSensor();
            return sensorStarted;
        }

        mCurrentPollingFrequency = frequency;
        return sensorStarted;
    }

    private void unregisterListener() {
        // Do not unregister if current polling frequency is 0, not polling for data.
        if (mCurrentPollingFrequency == 0) return;
        mProvider.getSensorManager().unregisterListener(this, mSensor);
    }

    /**
     * Requests sensor to stop polling for data.
     */
    @CalledByNative
    protected void stopSensor() {
        unregisterListener();
        mProvider.sensorStopped(this);
        mCurrentPollingFrequency = 0;
    }

    /**
     * Checks whether configuration is supported by the sensor. Currently only frequency is
     * supported.
     *
     * @return boolean true if configuration is supported, false otherwise.
     */
    @CalledByNative
    protected boolean checkSensorConfiguration(double frequency) {
        return mMinDelayUsec <= getSamplingPeriod(frequency);
    }

    /**
     * Converts frequency to sampling period in microseconds.
     */
    private int getSamplingPeriod(double frequency) {
        return (int) ((1 / frequency) * MICROSECONDS_IN_SECOND);
    }

    /**
     * Notifies native device::PlatformSensorAndroid when reading is changed.
     */
    protected void sensorReadingChanged() {
        nativeNotifyPlatformSensorReadingChanged(mNativePlatformSensorAndroid);
    }

    /**
     * Notifies native device::PlatformSensorAndroid when there is an error.
     */
    protected void sensorError() {
        nativeNotifyPlatformSensorError(mNativePlatformSensorAndroid);
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {}

    @Override
    public void onSensorChanged(SensorEvent event) {
        try {
            mSensorReadingData.mark();
            // Convert timestamp from nanoseconds to milliseconds.
            mSensorReadingData.putDouble(event.timestamp * MILLISECONDS_IN_NANOSECOND);
            fillSensorReadingData(event, mSensorReadingData);

            mSensorReadingData.reset();

            mBuffer.mark();
            mBuffer.put(mSensorReadingData);
            mSensorReadingData.reset();
            mBuffer.reset();

            if (getReportingMode() == ReportingMode.ON_CHANGE) {
                sensorReadingChanged();
            }
        } catch (BufferOverflowException | IllegalStateException e) {
            sensorError();
            stopSensor();
        }
    }

    private native void nativeNotifyPlatformSensorReadingChanged(long nativePlatformSensorAndroid);
    private native void nativeNotifyPlatformSensorError(long nativePlatformSensorAndroid);
}
