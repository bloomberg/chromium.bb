// Copyright 2014 The Chromium Authors. All rights reserved.
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
 * Test suite for DeviceSensors.
 */
public class DeviceSensorsTest extends AndroidTestCase {

    private DeviceSensorsForTests mDeviceSensors;
    private MockSensorManager mMockSensorManager;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mMockSensorManager = new MockSensorManager();
        mDeviceSensors = DeviceSensorsForTests.getInstance(getContext());
        mDeviceSensors.setSensorManagerProxy(mMockSensorManager);
    }

    @SmallTest
    public void testRegisterSensorsDeviceMotion() {
        boolean start = mDeviceSensors.start(0, DeviceSensors.DEVICE_MOTION, 100);

        assertTrue(start);
        assertTrue("should contain all motion sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_MOTION_SENSORS));
        assertTrue(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);

        assertEquals(DeviceSensors.DEVICE_MOTION_SENSORS.size(),
                mMockSensorManager.mNumRegistered);
        assertEquals(0, mMockSensorManager.mNumUnRegistered);
        assertEquals(DeviceSensors.DEVICE_MOTION_SENSORS.size(),
                mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    @SmallTest
    public void testRegisterSensorsDeviceOrientation() {
        boolean start = mDeviceSensors.start(0, DeviceSensors.DEVICE_ORIENTATION, 100);

        assertTrue(start);
        assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS));
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
        assertTrue(mDeviceSensors.mDeviceOrientationIsActive);

        assertEquals(DeviceSensors.DEVICE_ORIENTATION_SENSORS.size(),
                mMockSensorManager.mNumRegistered);
        assertEquals(0, mMockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testRegisterSensorsDeviceMotionAndOrientation() {
        boolean startOrientation = mDeviceSensors.start(0, DeviceSensors.DEVICE_ORIENTATION, 100);
        boolean startMotion = mDeviceSensors.start(0, DeviceSensors.DEVICE_MOTION, 100);

        assertTrue(startOrientation);
        assertTrue(startMotion);
        assertTrue("should contain all motion sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_MOTION_SENSORS));
        assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS));

        Set<Integer> union = new HashSet<Integer>(
            DeviceSensors.DEVICE_ORIENTATION_SENSORS);
        union.addAll(DeviceSensors.DEVICE_MOTION_SENSORS);

        assertEquals(union.size(), mDeviceSensors.mActiveSensors.size());
        assertTrue(mDeviceSensors.mDeviceMotionIsActive);
        assertTrue(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
        assertEquals(union.size(), mMockSensorManager.mNumRegistered);
        assertEquals(0, mMockSensorManager.mNumUnRegistered);
        assertEquals(DeviceSensors.DEVICE_MOTION_SENSORS.size(),
                mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    @SmallTest
    public void testRegisterSensorsDeviceLight() {
        boolean start = mDeviceSensors.start(0, DeviceSensors.DEVICE_LIGHT, 100);

        assertTrue(start);
        assertTrue(mDeviceSensors.mDeviceLightIsActive);
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);

        assertEquals(DeviceSensors.DEVICE_LIGHT_SENSORS.size(),
                mMockSensorManager.mNumRegistered);
        assertEquals(0, mMockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testUnregisterSensorsDeviceMotion() {
        mDeviceSensors.start(0, DeviceSensors.DEVICE_MOTION, 100);
        mDeviceSensors.stop(DeviceSensors.DEVICE_MOTION);

        assertTrue("should contain no sensors",
                mDeviceSensors.mActiveSensors.isEmpty());
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
        assertEquals(DeviceSensors.DEVICE_MOTION_SENSORS.size(),
                mMockSensorManager.mNumUnRegistered);
        assertEquals(0, mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    @SmallTest
    public void testUnregisterSensorsDeviceOrientation() {
        mDeviceSensors.start(0, DeviceSensors.DEVICE_ORIENTATION, 100);
        mDeviceSensors.stop(DeviceSensors.DEVICE_ORIENTATION);

        assertTrue("should contain no sensors",
                mDeviceSensors.mActiveSensors.isEmpty());
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
        assertEquals(DeviceSensors.DEVICE_ORIENTATION_SENSORS.size(),
                mMockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testUnRegisterSensorsDeviceMotionAndOrientation() {
        mDeviceSensors.start(0, DeviceSensors.DEVICE_ORIENTATION, 100);
        mDeviceSensors.start(0, DeviceSensors.DEVICE_MOTION, 100);
        mDeviceSensors.stop(DeviceSensors.DEVICE_MOTION);

        assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS));

        Set<Integer> diff = new HashSet<Integer>(DeviceSensors.DEVICE_MOTION_SENSORS);
        diff.removeAll(DeviceSensors.DEVICE_ORIENTATION_SENSORS);

        assertEquals(diff.size(), mMockSensorManager.mNumUnRegistered);

        mDeviceSensors.stop(DeviceSensors.DEVICE_ORIENTATION);

        assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        assertEquals(diff.size() + DeviceSensors.DEVICE_ORIENTATION_SENSORS.size(),
                mMockSensorManager.mNumUnRegistered);
        assertEquals(0, mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    @SmallTest
    public void testUnregisterSensorsLight() {
        mDeviceSensors.start(0, DeviceSensors.DEVICE_LIGHT, 100);
        mDeviceSensors.stop(DeviceSensors.DEVICE_LIGHT);

        assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
    }

    @SmallTest
    public void testSensorChangedgotLight() {
        boolean startLight = mDeviceSensors.start(0, DeviceSensors.DEVICE_LIGHT, 100);

        assertTrue(startLight);
        assertTrue(mDeviceSensors.mDeviceLightIsActive);

        float[] values = {200};
        mDeviceSensors.sensorChanged(Sensor.TYPE_LIGHT, values);
        mDeviceSensors.verifyCalls("gotLight");
        mDeviceSensors.verifyValue(200);
    }

    @SmallTest
    public void testSensorChangedgotOrientation() {
        boolean startOrientation = mDeviceSensors.start(0, DeviceSensors.DEVICE_ORIENTATION, 100);

        assertTrue(startOrientation);
        assertTrue(mDeviceSensors.mDeviceOrientationIsActive);

        float alpha = (float) Math.PI / 4;
        float[] values = {0, 0, (float) Math.sin(alpha / 2), (float) Math.cos(alpha / 2), -1};
        mDeviceSensors.sensorChanged(Sensor.TYPE_ROTATION_VECTOR, values);
        mDeviceSensors.verifyCalls("gotOrientation");
        mDeviceSensors.verifyValuesEpsilon(Math.toDegrees(alpha), 0, 0);
    }

    @SmallTest
    public void testSensorChangedgotAccelerationIncludingGravity() {
        mDeviceSensors.start(0, DeviceSensors.DEVICE_MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_ACCELEROMETER, values);
        mDeviceSensors.verifyCalls("gotAccelerationIncludingGravity");
        mDeviceSensors.verifyValues(1, 2, 3);
    }

    @SmallTest
    public void testSensorChangedgotAcceleration() {
        mDeviceSensors.start(0, DeviceSensors.DEVICE_MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_LINEAR_ACCELERATION, values);
        mDeviceSensors.verifyCalls("gotAcceleration");
        mDeviceSensors.verifyValues(1, 2, 3);
    }

    @SmallTest
    public void testSensorChangedgotRotationRate() {
        mDeviceSensors.start(0, DeviceSensors.DEVICE_MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_GYROSCOPE, values);
        mDeviceSensors.verifyCalls("gotRotationRate");
        mDeviceSensors.verifyValues(1, 2, 3);
    }

    @SmallTest
    public void testSensorChangedgotOrientationAndAcceleration() {
        boolean startOrientation = mDeviceSensors.start(0, DeviceSensors.DEVICE_ORIENTATION, 100);
        boolean startMotion = mDeviceSensors.start(0, DeviceSensors.DEVICE_MOTION, 100);

        assertTrue(startOrientation);
        assertTrue(startMotion);
        assertTrue(mDeviceSensors.mDeviceMotionIsActive);
        assertTrue(mDeviceSensors.mDeviceOrientationIsActive);

        float alpha = (float) Math.PI / 4;
        float[] values = {0, 0, (float) Math.sin(alpha / 2), (float) Math.cos(alpha / 2), -1};
        mDeviceSensors.sensorChanged(Sensor.TYPE_ROTATION_VECTOR, values);
        mDeviceSensors.verifyCalls("gotOrientation");
        mDeviceSensors.verifyValuesEpsilon(Math.toDegrees(alpha), 0, 0);

        float[] values2 = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_ACCELEROMETER, values2);
        mDeviceSensors.verifyCalls("gotOrientation" + "gotAccelerationIncludingGravity");
        mDeviceSensors.verifyValues(1, 2, 3);
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
        float[] r = new float[9];
        double[] values = new double[3];
        SensorManager.getRotationMatrix(r, null, gravity, magnetic);
        mDeviceSensors.computeDeviceOrientationFromRotationMatrix(r, values);

        assertEquals(expectedAngles.length, values.length);
        final double epsilon = 0.001;
        for (int i = 0; i < expectedAngles.length; ++i) {
            assertEquals(expectedAngles[i], values[i], epsilon);
        }

    }

    // -- End Tests for correct Device Orientation angles.

    private static class DeviceSensorsForTests extends DeviceSensors {

        private double mValue1 = 0;
        private double mValue2 = 0;
        private double mValue3 = 0;
        private String mCalls = "";

        private DeviceSensorsForTests(Context context) {
            super(context);
        }

        static DeviceSensorsForTests getInstance(Context context) {
            return new DeviceSensorsForTests(context);
        }

        private void verifyValue(double v1) {
            assertEquals(v1, mValue1);
        }

        private void verifyValues(double v1, double v2, double v3) {
            assertEquals(v1, mValue1);
            assertEquals(v2, mValue2);
            assertEquals(v3, mValue3);
        }

        private void verifyValuesEpsilon(double v1, double v2, double v3) {
            assertEquals(v1, mValue1, 0.1);
            assertEquals(v2, mValue2, 0.1);
            assertEquals(v3, mValue3, 0.1);
        }

        private void verifyCalls(String names) {
            assertEquals(mCalls, names);
        }

        @Override
        protected void gotLight(double light) {
            mValue1 = light;
            mCalls = mCalls.concat("gotLight");
        }

        @Override
        protected void gotOrientation(double alpha, double beta, double gamma) {
            mValue1 = alpha;
            mValue2 = beta;
            mValue3 = gamma;
            mCalls = mCalls.concat("gotOrientation");
        }

        @Override
        protected void gotAcceleration(double x, double y, double z) {
            mValue1 = x;
            mValue2 = y;
            mValue3 = z;
            mCalls = mCalls.concat("gotAcceleration");
        }

        @Override
        protected void gotAccelerationIncludingGravity(double x, double y, double z) {
            mValue1 = x;
            mValue2 = y;
            mValue3 = z;
            mCalls = mCalls.concat("gotAccelerationIncludingGravity");
        }

        @Override
        protected void gotRotationRate(double alpha, double beta, double gamma) {
            mValue1 = alpha;
            mValue2 = beta;
            mValue3 = gamma;
            mCalls = mCalls.concat("gotRotationRate");
        }
    }

    private static class MockSensorManager implements
            DeviceSensors.SensorManagerProxy {

        private int mNumRegistered = 0;
        private int mNumUnRegistered = 0;

        private MockSensorManager() {
        }

        @Override
        public boolean registerListener(SensorEventListener listener, int sensorType, int rate,
                Handler handler) {
            mNumRegistered++;
            return true;
        }

        @Override
        public void unregisterListener(SensorEventListener listener, int sensorType) {
            mNumUnRegistered++;
        }
    }
}
