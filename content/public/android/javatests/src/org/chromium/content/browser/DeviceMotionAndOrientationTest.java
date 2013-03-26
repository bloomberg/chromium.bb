// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.ActivityStatus;
import org.chromium.base.test.util.Feature;
import org.chromium.content.app.LibraryLoader;
import org.chromium.content.common.CommandLine;
import org.chromium.content.common.ProcessInitException;
import org.chromium.content_shell_apk.ContentShellApplication;

import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.test.UiThreadTest;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;

import com.google.common.collect.Sets;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;


/**
 * Test suite for DeviceMotionAndOrientation.
 */
public class DeviceMotionAndOrientationTest extends AndroidTestCase {

    private DeviceMotionAndOrientationForTests mDeviceMotionAndOrientation;
    private MockSensorManager mMockSensorManager;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mMockSensorManager = new MockSensorManager();
        mDeviceMotionAndOrientation = DeviceMotionAndOrientationForTests.getInstance();
        mDeviceMotionAndOrientation.setSensorManagerProxy(mMockSensorManager);
    }

    @SmallTest
    public void testRegisterSensorsDeviceMotion() {
        boolean start = mDeviceMotionAndOrientation.start(0,
                DeviceMotionAndOrientation.DEVICE_MOTION, 100);

        assertTrue(start);
        assertTrue("should contain all motion sensors",
                mDeviceMotionAndOrientation.mActiveSensors.containsAll(
                        DeviceMotionAndOrientation.DEVICE_MOTION_SENSORS));
        assertTrue(mDeviceMotionAndOrientation.mDeviceMotionIsActive);
        assertFalse(mDeviceMotionAndOrientation.mDeviceOrientationIsActive);

        assertEquals(DeviceMotionAndOrientation.DEVICE_MOTION_SENSORS.size(),
                mMockSensorManager.numRegistered);
        assertEquals(0, mMockSensorManager.numUnRegistered);
    }

    @SmallTest
    public void testRegisterSensorsDeviceOrientation() {
        boolean start = mDeviceMotionAndOrientation.start(0,
                DeviceMotionAndOrientation.DEVICE_ORIENTATION, 100);

        assertTrue(start);
        assertTrue("should contain all orientation sensors",
                mDeviceMotionAndOrientation.mActiveSensors.containsAll(
                        DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS));
        assertFalse(mDeviceMotionAndOrientation.mDeviceMotionIsActive);
        assertTrue(mDeviceMotionAndOrientation.mDeviceOrientationIsActive);

        assertEquals(DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS.size(),
                mMockSensorManager.numRegistered);
        assertEquals(0, mMockSensorManager.numUnRegistered);
    }

    @SmallTest
    public void testRegisterSensorsDeviceMotionAndOrientation() {
        boolean startOrientation = mDeviceMotionAndOrientation.start(0,
                DeviceMotionAndOrientation.DEVICE_ORIENTATION, 100);
        boolean startMotion = mDeviceMotionAndOrientation.start(0,
                DeviceMotionAndOrientation.DEVICE_MOTION, 100);

        assertTrue(startOrientation);
        assertTrue(startMotion);
        assertTrue("should contain all motion sensors",
                mDeviceMotionAndOrientation.mActiveSensors.containsAll(
                        DeviceMotionAndOrientation.DEVICE_MOTION_SENSORS));
        assertTrue("should contain all orientation sensors",
                mDeviceMotionAndOrientation.mActiveSensors.containsAll(
                        DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS));

        Set<Integer> union = Sets.newHashSet(DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS);
        union.addAll(DeviceMotionAndOrientation.DEVICE_MOTION_SENSORS);

        assertEquals(union.size(), mDeviceMotionAndOrientation.mActiveSensors.size());
        assertTrue(mDeviceMotionAndOrientation.mDeviceMotionIsActive);
        assertTrue(mDeviceMotionAndOrientation.mDeviceOrientationIsActive);
        assertEquals(union.size(), mMockSensorManager.numRegistered);
        assertEquals(0, mMockSensorManager.numUnRegistered);
    }

    @SmallTest
    public void testUnregisterSensorsDeviceMotion() {
        mDeviceMotionAndOrientation.start(0, DeviceMotionAndOrientation.DEVICE_MOTION, 100);
        mDeviceMotionAndOrientation.stop(DeviceMotionAndOrientation.DEVICE_MOTION);

        assertTrue("should contain no sensors",
                mDeviceMotionAndOrientation.mActiveSensors.isEmpty());
        assertFalse(mDeviceMotionAndOrientation.mDeviceMotionIsActive);
        assertFalse(mDeviceMotionAndOrientation.mDeviceOrientationIsActive);
        assertEquals(DeviceMotionAndOrientation.DEVICE_MOTION_SENSORS.size(),
                mMockSensorManager.numUnRegistered);
    }

    @SmallTest
    public void testUnregisterSensorsDeviceOrientation() {
        mDeviceMotionAndOrientation.start(0, DeviceMotionAndOrientation.DEVICE_ORIENTATION, 100);
        mDeviceMotionAndOrientation.stop(DeviceMotionAndOrientation.DEVICE_ORIENTATION);

        assertTrue("should contain no sensors",
                mDeviceMotionAndOrientation.mActiveSensors.isEmpty());
        assertFalse(mDeviceMotionAndOrientation.mDeviceMotionIsActive);
        assertFalse(mDeviceMotionAndOrientation.mDeviceOrientationIsActive);
        assertEquals(DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS.size(),
                mMockSensorManager.numUnRegistered);
    }

    @SmallTest
    public void testUnRegisterSensorsDeviceMotionAndOrientation() {
        mDeviceMotionAndOrientation.start(0, DeviceMotionAndOrientation.DEVICE_ORIENTATION, 100);
        mDeviceMotionAndOrientation.start(0, DeviceMotionAndOrientation.DEVICE_MOTION, 100);
        mDeviceMotionAndOrientation.stop(DeviceMotionAndOrientation.DEVICE_MOTION);

        assertTrue("should contain all orientation sensors",
                mDeviceMotionAndOrientation.mActiveSensors.containsAll(
                        DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS));

        Set<Integer> diff = Sets.newHashSet(DeviceMotionAndOrientation.DEVICE_MOTION_SENSORS);
        diff.removeAll(DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS);

        assertEquals(diff.size(), mMockSensorManager.numUnRegistered);

        mDeviceMotionAndOrientation.stop(DeviceMotionAndOrientation.DEVICE_ORIENTATION);

        assertTrue("should contain no sensors",
                mDeviceMotionAndOrientation.mActiveSensors.isEmpty());
        assertEquals(diff.size() + DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS.size(),
                mMockSensorManager.numUnRegistered);
    }

    @SmallTest
    public void testSensorChangedgotAccelerationAndOrientation() {
        boolean startOrientation = mDeviceMotionAndOrientation.start(0,
                DeviceMotionAndOrientation.DEVICE_ORIENTATION, 100);
        boolean startMotion = mDeviceMotionAndOrientation.start(0,
                DeviceMotionAndOrientation.DEVICE_MOTION, 100);

        assertTrue(startOrientation);
        assertTrue(startMotion);
        assertTrue(mDeviceMotionAndOrientation.mDeviceMotionIsActive);
        assertTrue(mDeviceMotionAndOrientation.mDeviceOrientationIsActive);

        float[] values = {0.0f, 0.0f, 9.0f};
        float[] values2 = {10.0f, 10.0f, 10.0f};
        mDeviceMotionAndOrientation.sensorChanged(Sensor.TYPE_ACCELEROMETER, values);
        mDeviceMotionAndOrientation.sensorChanged(Sensor.TYPE_MAGNETIC_FIELD, values2);
        mDeviceMotionAndOrientation.verifyCalls("gotAccelerationIncludingGravity" +
                "gotOrientation");
        mDeviceMotionAndOrientation.verifyValuesEpsilon(45, 0, 0);
    }

    @SmallTest
    public void testSensorChangedgotAccelerationIncludingGravity() {
        mDeviceMotionAndOrientation.start(0, DeviceMotionAndOrientation.DEVICE_MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceMotionAndOrientation.sensorChanged(Sensor.TYPE_ACCELEROMETER, values);
        mDeviceMotionAndOrientation.verifyCalls("gotAccelerationIncludingGravity");
        mDeviceMotionAndOrientation.verifyValues(1, 2, 3);
    }

    @SmallTest
    public void testSensorChangedgotAcceleration() {
        mDeviceMotionAndOrientation.start(0, DeviceMotionAndOrientation.DEVICE_MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceMotionAndOrientation.sensorChanged(Sensor.TYPE_LINEAR_ACCELERATION, values);
        mDeviceMotionAndOrientation.verifyCalls("gotAcceleration");
        mDeviceMotionAndOrientation.verifyValues(1, 2, 3);
    }

    @SmallTest
    public void testSensorChangedgotRotationRate() {
        mDeviceMotionAndOrientation.start(0, DeviceMotionAndOrientation.DEVICE_MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceMotionAndOrientation.sensorChanged(Sensor.TYPE_GYROSCOPE, values);
        mDeviceMotionAndOrientation.verifyCalls("gotRotationRate");
        mDeviceMotionAndOrientation.verifyValues(1, 2, 3);
    }

    @SmallTest
    public void testSensorChangedmagneticField() {
        mDeviceMotionAndOrientation.start(0, DeviceMotionAndOrientation.DEVICE_ORIENTATION, 100);

        float[] values = {1, 2, 3};
        mDeviceMotionAndOrientation.sensorChanged(Sensor.TYPE_MAGNETIC_FIELD, values);
        mDeviceMotionAndOrientation.verifyCalls("");
    }

    private static class DeviceMotionAndOrientationForTests extends DeviceMotionAndOrientation {

        private double value1 = 0;
        private double value2 = 0;
        private double value3 = 0;
        private String mCalls = "";

        private DeviceMotionAndOrientationForTests(){
        }

        static DeviceMotionAndOrientationForTests getInstance() {
            return new DeviceMotionAndOrientationForTests();
        }

        private void verifyValues(double v1, double v2, double v3) {
            assertEquals(v1, value1);
            assertEquals(v2, value2);
            assertEquals(v3, value3);
        }

        private void verifyValuesEpsilon(double v1, double v2, double v3) {
            assertEquals(v1, value1, 0.1);
            assertEquals(v2, value2, 0.1);
            assertEquals(v3, value3, 0.1);
        }

        private void verifyCalls(String names) {
            assertEquals(mCalls, names);
        }

        @Override
        protected void gotOrientation(double alpha, double beta, double gamma) {
            value1 = alpha;
            value2 = beta;
            value3 = gamma;
            mCalls = mCalls.concat("gotOrientation");
        }

        @Override
        protected void gotAcceleration(double x, double y, double z) {
            value1 = x;
            value2 = y;
            value3 = z;
            mCalls = mCalls.concat("gotAcceleration");
        }

        @Override
        protected void gotAccelerationIncludingGravity(double x, double y, double z) {
            value1 = x;
            value2 = y;
            value3 = z;
            mCalls = mCalls.concat("gotAccelerationIncludingGravity");
        }

        @Override
        protected void gotRotationRate(double alpha, double beta, double gamma) {
            value1 = alpha;
            value2 = beta;
            value3 = gamma;
            mCalls = mCalls.concat("gotRotationRate");
        }
    }

    private static class MockSensorManager implements
            DeviceMotionAndOrientation.SensorManagerProxy {

        private int numRegistered = 0;
        private int numUnRegistered = 0;

        private MockSensorManager() {
        }

        @Override
        public boolean registerListener(SensorEventListener listener, int sensorType, int rate,
                Handler handler) {
            numRegistered++;
            return true;
        }

        @Override
        public void unregisterListener(SensorEventListener listener, int sensorType) {
            numUnRegistered++;
        }
    }
}
