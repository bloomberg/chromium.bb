// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import java.util.HashSet;
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
        mDeviceMotionAndOrientation = DeviceMotionAndOrientationForTests.getInstance(getContext());
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
        assertEquals(DeviceMotionAndOrientation.DEVICE_MOTION_SENSORS.size(),
                mDeviceMotionAndOrientation.getNumberActiveDeviceMotionSensors());
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

        Set<Integer> union = new HashSet<Integer>(
            DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS);
        union.addAll(DeviceMotionAndOrientation.DEVICE_MOTION_SENSORS);

        assertEquals(union.size(), mDeviceMotionAndOrientation.mActiveSensors.size());
        assertTrue(mDeviceMotionAndOrientation.mDeviceMotionIsActive);
        assertTrue(mDeviceMotionAndOrientation.mDeviceOrientationIsActive);
        assertEquals(union.size(), mMockSensorManager.numRegistered);
        assertEquals(0, mMockSensorManager.numUnRegistered);
        assertEquals(DeviceMotionAndOrientation.DEVICE_MOTION_SENSORS.size(),
                mDeviceMotionAndOrientation.getNumberActiveDeviceMotionSensors());
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
        assertEquals(0, mDeviceMotionAndOrientation.getNumberActiveDeviceMotionSensors());
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

        Set<Integer> diff = new HashSet<Integer>(DeviceMotionAndOrientation.DEVICE_MOTION_SENSORS);
        diff.removeAll(DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS);

        assertEquals(diff.size(), mMockSensorManager.numUnRegistered);

        mDeviceMotionAndOrientation.stop(DeviceMotionAndOrientation.DEVICE_ORIENTATION);

        assertTrue("should contain no sensors",
                mDeviceMotionAndOrientation.mActiveSensors.isEmpty());
        assertEquals(diff.size() + DeviceMotionAndOrientation.DEVICE_ORIENTATION_SENSORS.size(),
                mMockSensorManager.numUnRegistered);
        assertEquals(0, mDeviceMotionAndOrientation.getNumberActiveDeviceMotionSensors());
    }

    @SmallTest
    public void testSensorChangedgotOrientation() {
        boolean startOrientation = mDeviceMotionAndOrientation.start(0,
                DeviceMotionAndOrientation.DEVICE_ORIENTATION, 100);

        assertTrue(startOrientation);
        assertTrue(mDeviceMotionAndOrientation.mDeviceOrientationIsActive);

        float alpha = (float) Math.PI / 4;
        float[] values = {0, 0, (float) Math.sin(alpha / 2), (float) Math.cos(alpha / 2), -1};
        mDeviceMotionAndOrientation.sensorChanged(Sensor.TYPE_ROTATION_VECTOR, values);
        mDeviceMotionAndOrientation.verifyCalls("gotOrientation");
        mDeviceMotionAndOrientation.verifyValuesEpsilon(Math.toDegrees(alpha), 0, 0);
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
    public void testSensorChangedgotOrientationAndAcceleration() {
        boolean startOrientation = mDeviceMotionAndOrientation.start(0,
                DeviceMotionAndOrientation.DEVICE_ORIENTATION, 100);
        boolean startMotion = mDeviceMotionAndOrientation.start(0,
                DeviceMotionAndOrientation.DEVICE_MOTION, 100);

        assertTrue(startOrientation);
        assertTrue(startMotion);
        assertTrue(mDeviceMotionAndOrientation.mDeviceMotionIsActive);
        assertTrue(mDeviceMotionAndOrientation.mDeviceOrientationIsActive);

        float alpha = (float) Math.PI / 4;
        float[] values = {0, 0, (float) Math.sin(alpha / 2), (float) Math.cos(alpha / 2), -1};
        mDeviceMotionAndOrientation.sensorChanged(Sensor.TYPE_ROTATION_VECTOR, values);
        mDeviceMotionAndOrientation.verifyCalls("gotOrientation");
        mDeviceMotionAndOrientation.verifyValuesEpsilon(Math.toDegrees(alpha), 0, 0);

        float[] values2 = {1, 2, 3};
        mDeviceMotionAndOrientation.sensorChanged(Sensor.TYPE_ACCELEROMETER, values2);
        mDeviceMotionAndOrientation.verifyCalls("gotOrientation" +
                "gotAccelerationIncludingGravity");
        mDeviceMotionAndOrientation.verifyValues(1, 2, 3);
    }


    // Tests for correct Device Orientation angles.

    @SmallTest
    public void testOrientationAnglesFromRotationMatrixIdentity() {
        float[] gravity = {0, 0, 1};
        float[] magnetic = {0, 1, 0};
        double[] expectedAngles = {0, 0, 0};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @SmallTest
    public void testOrientationAnglesFromRotationMatrix45DegreesX() {
        float[] gravity = {0, (float) Math.sin(Math.PI / 4), (float) Math.cos(Math.PI / 4)};
        float[] magnetic = {0, 1, 0};
        double[] expectedAngles = {0, Math.PI / 4, 0};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @SmallTest
    public void testOrientationAnglesFromRotationMatrix45DegreesY() {
        float[] gravity = {-(float) Math.sin(Math.PI / 4), 0, (float) Math.cos(Math.PI / 4)};
        float[] magnetic = {0, 1, 0};
        double[] expectedAngles = {0, 0, Math.PI / 4};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @SmallTest
    public void testOrientationAnglesFromRotationMatrix45DegreesZ() {
        float[] gravity = {0, 0, 1};
        float[] magnetic = {(float) Math.sin(Math.PI / 4), (float) Math.cos(Math.PI / 4), 0};
        double[] expectedAngles = {Math.PI / 4, 0, 0};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @SmallTest
    public void testOrientationAnglesFromRotationMatrixGimbalLock() {
        float[] gravity = {0, 1, 0};
        float[] magnetic = {(float) Math.sin(Math.PI / 4), 0, -(float) Math.cos(Math.PI / 4)};
        double[] expectedAngles = {Math.PI / 4, Math.PI / 2, 0};  // favor yaw instead of roll

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @SmallTest
    public void testOrientationAnglesFromRotationMatrixPitchGreaterThan90() {
        final double largePitchAngle = Math.PI / 2 + Math.PI / 4;
        float[] gravity = {0, (float) Math.cos(largePitchAngle - Math.PI / 2),
                -(float) Math.sin(largePitchAngle - Math.PI / 2)};
        float[] magnetic = {0, 0, -1};
        double[] expectedAngles = {0, largePitchAngle, 0};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @SmallTest
    public void testOrientationAnglesFromRotationMatrixRoll90() {
        float[] gravity = {-1, 0, 0};
        float[] magnetic = {0, 1, 0};
        double[] expectedAngles = {Math.PI, -Math.PI, -Math.PI / 2};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    /**
     * Helper method for verifying angles obtained from rotation matrix.
     *
     * @param gravity
     *        gravity vector in the device frame
     * @param magnetic
     *        magnetic field vector in the device frame
     * @param expectedAngles
     *        expectedAngles[0] rotation angle in radians around the Z-axis
     *        expectedAngles[1] rotation angle in radians around the X-axis
     *        expectedAngles[2] rotation angle in radians around the Y-axis
     */
    private void verifyOrientationAngles(float[] gravity, float[] magnetic,
            double[] expectedAngles) {
        float[] R = new float[9];
        double[] values = new double[3];
        SensorManager.getRotationMatrix(R, null, gravity, magnetic);
        mDeviceMotionAndOrientation.computeDeviceOrientationFromRotationMatrix(R, values);

        assertEquals(expectedAngles.length, values.length);
        final double epsilon = 0.001;
        for (int i = 0; i < expectedAngles.length; ++i) {
            assertEquals(expectedAngles[i], values[i], epsilon);
        }

    }

    // -- End Tests for correct Device Orientation angles.

    private static class DeviceMotionAndOrientationForTests extends DeviceMotionAndOrientation {

        private double value1 = 0;
        private double value2 = 0;
        private double value3 = 0;
        private String mCalls = "";

        private DeviceMotionAndOrientationForTests(Context context) {
            super(context);
        }

        static DeviceMotionAndOrientationForTests getInstance(Context context) {
            return new DeviceMotionAndOrientationForTests(context);
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
