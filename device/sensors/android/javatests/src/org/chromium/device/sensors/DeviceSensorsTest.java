// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.sensors;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.util.HashSet;
import java.util.Set;

/**
 * Test suite for DeviceSensors.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class DeviceSensorsTest {
    private DeviceSensorsForTests mDeviceSensors;
    private MockSensorManager mMockSensorManager;

    @Before
    public void setUp() throws Exception {
        mMockSensorManager = new MockSensorManager();
        mDeviceSensors = DeviceSensorsForTests.getInstance(InstrumentationRegistry.getContext());
        mDeviceSensors.setSensorManagerProxy(mMockSensorManager);
    }

    @Test
    @SmallTest
    public void testRegisterSensorsDeviceMotion() {
        boolean start = mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        Assert.assertTrue(start);
        Assert.assertTrue("should contain all motion sensors",
                mDeviceSensors.mActiveSensors.containsAll(DeviceSensors.DEVICE_MOTION_SENSORS));
        Assert.assertTrue(mDeviceSensors.mDeviceMotionIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActive);

        Assert.assertEquals(
                DeviceSensors.DEVICE_MOTION_SENSORS.size(), mMockSensorManager.mNumRegistered);
        Assert.assertEquals(0, mMockSensorManager.mNumUnRegistered);
        Assert.assertEquals(DeviceSensors.DEVICE_MOTION_SENSORS.size(),
                mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    @Test
    @SmallTest
    public void testRegisterSensorsDeviceOrientation() {
        boolean start = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);

        Assert.assertTrue(start);
        Assert.assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_A));
        Assert.assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        Assert.assertEquals(OrientationSensorType.GAME_ROTATION_VECTOR,
                mDeviceSensors.getOrientationSensorTypeUsed());

        Assert.assertEquals(DeviceSensors.DEVICE_ORIENTATION_SENSORS_A.size(),
                mMockSensorManager.mNumRegistered);
        Assert.assertEquals(0, mMockSensorManager.mNumUnRegistered);
    }

    @Test
    @SmallTest
    public void testRegisterSensorsDeviceOrientationGameRotationVectorNotAvailable() {
        MockSensorManager mockSensorManager = new MockSensorManager();
        mockSensorManager.setGameRotationVectorAvailable(false);
        mDeviceSensors.setSensorManagerProxy(mockSensorManager);
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);

        Assert.assertTrue(startOrientation);
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        Assert.assertTrue("should contain option B orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_B));
        Assert.assertEquals(OrientationSensorType.ROTATION_VECTOR,
                mDeviceSensors.getOrientationSensorTypeUsed());

        Assert.assertEquals(DeviceSensors.DEVICE_ORIENTATION_SENSORS_B.size(),
                mockSensorManager.mNumRegistered);
        Assert.assertEquals(0, mockSensorManager.mNumUnRegistered);
    }

    @Test
    @SmallTest
    public void testRegisterSensorsDeviceOrientationBothRotationVectorsNotAvailable() {
        MockSensorManager mockSensorManager = new MockSensorManager();
        mockSensorManager.setGameRotationVectorAvailable(false);
        mockSensorManager.setRotationVectorAvailable(false);
        mDeviceSensors.setSensorManagerProxy(mockSensorManager);
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);

        Assert.assertTrue(startOrientation);
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        Assert.assertTrue("should contain option C orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_C));
        Assert.assertEquals(OrientationSensorType.ACCELEROMETER_MAGNETIC,
                mDeviceSensors.getOrientationSensorTypeUsed());

        Assert.assertEquals(DeviceSensors.DEVICE_ORIENTATION_SENSORS_C.size(),
                mockSensorManager.mNumRegistered);
        Assert.assertEquals(0, mockSensorManager.mNumUnRegistered);
    }

    @Test
    @SmallTest
    public void testRegisterSensorsDeviceOrientationNoSensorsAvailable() {
        MockSensorManager mockSensorManager = new MockSensorManager();
        mockSensorManager.setGameRotationVectorAvailable(false);
        mockSensorManager.setRotationVectorAvailable(false);
        mockSensorManager.setAccelerometerAvailable(false);
        mDeviceSensors.setSensorManagerProxy(mockSensorManager);
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);

        Assert.assertFalse(startOrientation);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        Assert.assertTrue(mDeviceSensors.mActiveSensors.isEmpty());
        Assert.assertEquals(
                OrientationSensorType.NOT_AVAILABLE, mDeviceSensors.getOrientationSensorTypeUsed());

        Assert.assertEquals(0, mockSensorManager.mNumRegistered);
        Assert.assertEquals(0, mockSensorManager.mNumUnRegistered);
    }

    @Test
    @SmallTest
    public void testRegisterSensorsDeviceMotionAndOrientation() {
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);
        boolean startMotion = mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        Assert.assertTrue(startOrientation);
        Assert.assertTrue(startMotion);
        Assert.assertTrue("should contain all motion sensors",
                mDeviceSensors.mActiveSensors.containsAll(DeviceSensors.DEVICE_MOTION_SENSORS));
        Assert.assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_A));

        Set<Integer> union = new HashSet<Integer>(DeviceSensors.DEVICE_ORIENTATION_SENSORS_A);
        union.addAll(DeviceSensors.DEVICE_MOTION_SENSORS);

        Assert.assertEquals(union.size(), mDeviceSensors.mActiveSensors.size());
        Assert.assertTrue(mDeviceSensors.mDeviceMotionIsActive);
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertEquals(union.size(), mMockSensorManager.mNumRegistered);
        Assert.assertEquals(0, mMockSensorManager.mNumUnRegistered);
        Assert.assertEquals(DeviceSensors.DEVICE_MOTION_SENSORS.size(),
                mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    @Test
    @SmallTest
    public void testRegisterSensorsDeviceOrientationAbsolute() {
        boolean start = mDeviceSensors.start(0, ConsumerType.ORIENTATION_ABSOLUTE, 100);

        Assert.assertTrue(start);
        Assert.assertTrue("should contain all absolute orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS_A));
        Assert.assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationAbsoluteIsActiveWithBackupSensors);
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationAbsoluteIsActive);

        Assert.assertEquals(DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS_A.size(),
                mMockSensorManager.mNumRegistered);
        Assert.assertEquals(0, mMockSensorManager.mNumUnRegistered);
    }

    @Test
    @SmallTest
    public void testUnregisterSensorsDeviceOrientationAbsolute() {
        mDeviceSensors.start(0, ConsumerType.ORIENTATION_ABSOLUTE, 100);
        mDeviceSensors.stop(ConsumerType.ORIENTATION_ABSOLUTE);

        Assert.assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        Assert.assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationAbsoluteIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationAbsoluteIsActiveWithBackupSensors);
        Assert.assertEquals(DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS_A.size(),
                mMockSensorManager.mNumUnRegistered);
    }

    @Test
    @SmallTest
    public void testRegisterSensorsDeviceOrientationAbsoluteRotationVectorNotAvailable() {
        MockSensorManager mockSensorManager = new MockSensorManager();
        mockSensorManager.setRotationVectorAvailable(false);
        mDeviceSensors.setSensorManagerProxy(mockSensorManager);
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION_ABSOLUTE, 100);

        Assert.assertTrue(startOrientation);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationAbsoluteIsActive);
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationAbsoluteIsActiveWithBackupSensors);
        Assert.assertTrue("should contain option B orientation absolute sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS_B));
        Assert.assertEquals(DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS_B.size(),
                mockSensorManager.mNumRegistered);
        Assert.assertEquals(0, mockSensorManager.mNumUnRegistered);
    }

    @Test
    @SmallTest
    public void testRegisterSensorsDeviceOrientationAndOrientationAbsolute() {
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);
        boolean startOrientationAbsolute =
                mDeviceSensors.start(0, ConsumerType.ORIENTATION_ABSOLUTE, 100);

        Assert.assertTrue(startOrientation);
        Assert.assertTrue(startOrientationAbsolute);
        Assert.assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_A));
        Assert.assertTrue("should contain all absolute orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS_A));

        Set<Integer> union = new HashSet<Integer>(DeviceSensors.DEVICE_ORIENTATION_SENSORS_A);
        union.addAll(DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS_A);

        Assert.assertEquals(union.size(), mDeviceSensors.mActiveSensors.size());
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationAbsoluteIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        Assert.assertEquals(union.size(), mMockSensorManager.mNumRegistered);
        Assert.assertEquals(0, mMockSensorManager.mNumUnRegistered);
    }

    @Test
    @SmallTest
    public void testUnregisterSensorsDeviceMotion() {
        mDeviceSensors.start(0, ConsumerType.MOTION, 100);
        mDeviceSensors.stop(ConsumerType.MOTION);

        Assert.assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        Assert.assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertEquals(
                DeviceSensors.DEVICE_MOTION_SENSORS.size(), mMockSensorManager.mNumUnRegistered);
        Assert.assertEquals(0, mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    @Test
    @SmallTest
    public void testUnregisterSensorsDeviceOrientation() {
        mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);
        mDeviceSensors.stop(ConsumerType.ORIENTATION);

        Assert.assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        Assert.assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        Assert.assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        Assert.assertEquals(DeviceSensors.DEVICE_ORIENTATION_SENSORS_A.size(),
                mMockSensorManager.mNumUnRegistered);
    }

    @Test
    @SmallTest
    public void testUnregisterSensorsDeviceMotionAndOrientation() {
        mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);
        mDeviceSensors.start(0, ConsumerType.MOTION, 100);
        mDeviceSensors.stop(ConsumerType.MOTION);

        Assert.assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_A));

        Set<Integer> diff = new HashSet<Integer>(DeviceSensors.DEVICE_MOTION_SENSORS);
        diff.removeAll(DeviceSensors.DEVICE_ORIENTATION_SENSORS_A);

        Assert.assertEquals(diff.size(), mMockSensorManager.mNumUnRegistered);

        mDeviceSensors.stop(ConsumerType.ORIENTATION);

        Assert.assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        Assert.assertEquals(diff.size() + DeviceSensors.DEVICE_ORIENTATION_SENSORS_A.size(),
                mMockSensorManager.mNumUnRegistered);
        Assert.assertEquals(0, mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    /**
     * Helper method to trigger an orientation change using the given sensorType.
     */
    private void changeOrientation(int sensorType, boolean absolute, String expectedChange) {
        boolean startOrientation = mDeviceSensors.start(
                0, absolute ? ConsumerType.ORIENTATION_ABSOLUTE : ConsumerType.ORIENTATION, 100);

        Assert.assertTrue(startOrientation);
        Assert.assertTrue(absolute ? mDeviceSensors.mDeviceOrientationAbsoluteIsActive
                                   : mDeviceSensors.mDeviceOrientationIsActive);

        float alpha = (float) Math.PI / 4;
        float[] values = {0, 0, (float) Math.sin(alpha / 2), (float) Math.cos(alpha / 2), -1};
        mDeviceSensors.sensorChanged(sensorType, values);

        mDeviceSensors.verifyCalls(expectedChange);
        if (!expectedChange.isEmpty()) {
            mDeviceSensors.verifyValuesEpsilon(Math.toDegrees(alpha), 0, 0);
        }
    }

    @Test
    @SmallTest
    public void testSensorChangedGotOrientationViaRotationVector() {
        changeOrientation(Sensor.TYPE_ROTATION_VECTOR, false /* absolute */, "");
    }

    @Test
    @SmallTest
    public void testSensorChangedGotOrientationViaGameRotationVector() {
        changeOrientation(Sensor.TYPE_GAME_ROTATION_VECTOR, false /* absolute */, "gotOrientation");
    }

    @Test
    @SmallTest
    public void testSensorChangedGotOrientationAbsoluteViaRotationVector() {
        changeOrientation(
                Sensor.TYPE_ROTATION_VECTOR, true /* absolute */, "gotOrientationAbsolute");
    }

    @Test
    @SmallTest
    public void testSensorChangedGotOrientationAbsoluteViaGameRotationVector() {
        changeOrientation(Sensor.TYPE_GAME_ROTATION_VECTOR, true /* absolute */, "");
    }

    @Test
    @SmallTest
    public void testSensorChangedGotOrientationAndOrientationAbsolute() {
        changeOrientation(Sensor.TYPE_GAME_ROTATION_VECTOR, false /* absolute */, "gotOrientation");
        changeOrientation(Sensor.TYPE_ROTATION_VECTOR, true /* absolute */, "gotOrientation"
                        + "gotOrientationAbsolute");
    }

    @Test
    @SmallTest
    public void testSensorChangedGotOrientationViaRotationVectorAndOrientationAbsolute() {
        MockSensorManager mockSensorManager = new MockSensorManager();
        mockSensorManager.setGameRotationVectorAvailable(false);
        mDeviceSensors.setSensorManagerProxy(mockSensorManager);

        changeOrientation(Sensor.TYPE_ROTATION_VECTOR, false /* absolute */, "gotOrientation");
        changeOrientation(Sensor.TYPE_ROTATION_VECTOR, true /* absolute */, "gotOrientation"
                        + "gotOrientationAbsolute"
                        + "gotOrientation");
    }

    @Test
    @SmallTest
    public void testSensorChangedGotAccelerationIncludingGravity() {
        mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_ACCELEROMETER, values);
        mDeviceSensors.verifyCalls("gotAccelerationIncludingGravity");
        mDeviceSensors.verifyValues(1, 2, 3);
    }

    @Test
    @SmallTest
    public void testSensorChangedGotAcceleration() {
        mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_LINEAR_ACCELERATION, values);
        mDeviceSensors.verifyCalls("gotAcceleration");
        mDeviceSensors.verifyValues(1, 2, 3);
    }

    @Test
    @SmallTest
    public void testSensorChangedGotRotationRate() {
        mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_GYROSCOPE, values);
        mDeviceSensors.verifyCalls("gotRotationRate");
        mDeviceSensors.verifyValues(1, 2, 3);
    }

    @Test
    @SmallTest
    public void testSensorChangedGotOrientationAndAcceleration() {
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);
        boolean startMotion = mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        Assert.assertTrue(startOrientation);
        Assert.assertTrue(startMotion);
        Assert.assertTrue(mDeviceSensors.mDeviceMotionIsActive);
        Assert.assertTrue(mDeviceSensors.mDeviceOrientationIsActive);

        float alpha = (float) Math.PI / 4;
        float[] values = {0, 0, (float) Math.sin(alpha / 2), (float) Math.cos(alpha / 2), -1};
        mDeviceSensors.sensorChanged(Sensor.TYPE_GAME_ROTATION_VECTOR, values);
        mDeviceSensors.verifyCalls("gotOrientation");
        mDeviceSensors.verifyValuesEpsilon(Math.toDegrees(alpha), 0, 0);

        float[] values2 = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_ACCELEROMETER, values2);
        mDeviceSensors.verifyCalls("gotOrientation"
                + "gotAccelerationIncludingGravity");
        mDeviceSensors.verifyValues(1, 2, 3);
    }

    // Tests for correct Device Orientation angles.

    @Test
    @SmallTest
    public void testOrientationAnglesFromRotationMatrixIdentity() {
        float[] gravity = {0, 0, 1};
        float[] magnetic = {0, 1, 0};
        double[] expectedAngles = {0, 0, 0};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @Test
    @SmallTest
    public void testOrientationAnglesFromRotationMatrix45DegreesX() {
        float[] gravity = {0, (float) Math.sin(Math.PI / 4), (float) Math.cos(Math.PI / 4)};
        float[] magnetic = {0, 1, 0};
        double[] expectedAngles = {0, Math.PI / 4, 0};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @Test
    @SmallTest
    public void testOrientationAnglesFromRotationMatrix45DegreesY() {
        float[] gravity = {-(float) Math.sin(Math.PI / 4), 0, (float) Math.cos(Math.PI / 4)};
        float[] magnetic = {0, 1, 0};
        double[] expectedAngles = {0, 0, Math.PI / 4};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @Test
    @SmallTest
    public void testOrientationAnglesFromRotationMatrix45DegreesZ() {
        float[] gravity = {0, 0, 1};
        float[] magnetic = {(float) Math.sin(Math.PI / 4), (float) Math.cos(Math.PI / 4), 0};
        double[] expectedAngles = {Math.PI / 4, 0, 0};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @Test
    @SmallTest
    public void testOrientationAnglesFromRotationMatrixGimbalLock() {
        float[] gravity = {0, 1, 0};
        float[] magnetic = {(float) Math.sin(Math.PI / 4), 0, -(float) Math.cos(Math.PI / 4)};
        double[] expectedAngles = {Math.PI / 4, Math.PI / 2, 0}; // favor yaw instead of roll

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @Test
    @SmallTest
    public void testOrientationAnglesFromRotationMatrixPitchGreaterThan90() {
        final double largePitchAngle = Math.PI / 2 + Math.PI / 4;
        float[] gravity = {0, (float) Math.cos(largePitchAngle - Math.PI / 2),
                -(float) Math.sin(largePitchAngle - Math.PI / 2)};
        float[] magnetic = {0, 0, -1};
        double[] expectedAngles = {0, largePitchAngle, 0};

        verifyOrientationAngles(gravity, magnetic, expectedAngles);
    }

    @Test
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
    private void verifyOrientationAngles(
            float[] gravity, float[] magnetic, double[] expectedAngles) {
        float[] r = new float[9];
        double[] values = new double[3];
        SensorManager.getRotationMatrix(r, null, gravity, magnetic);
        DeviceSensors.computeDeviceOrientationFromRotationMatrix(r, values);

        Assert.assertEquals(expectedAngles.length, values.length);
        final double epsilon = 0.001;
        for (int i = 0; i < expectedAngles.length; ++i) {
            Assert.assertEquals(expectedAngles[i], values[i], epsilon);
        }
    }

    // -- End Tests for correct Device Orientation angles.

    private static class DeviceSensorsForTests extends DeviceSensors {
        private double mValue1 = 0;
        private double mValue2 = 0;
        private double mValue3 = 0;
        private String mCalls = "";

        private DeviceSensorsForTests(Context context) {
            super();
        }

        static DeviceSensorsForTests getInstance(Context context) {
            return new DeviceSensorsForTests(context);
        }

        private void verifyValue(double v1) {
            Assert.assertEquals(v1, mValue1);
        }

        private void verifyValues(double v1, double v2, double v3) {
            Assert.assertEquals(v1, mValue1, 0);
            Assert.assertEquals(v2, mValue2, 0);
            Assert.assertEquals(v3, mValue3, 0);
        }

        private void verifyValuesEpsilon(double v1, double v2, double v3) {
            Assert.assertEquals(v1, mValue1, 0.1);
            Assert.assertEquals(v2, mValue2, 0.1);
            Assert.assertEquals(v3, mValue3, 0.1);
        }

        private void verifyCalls(String names) {
            Assert.assertEquals(mCalls, names);
        }

        @Override
        protected void gotOrientation(double alpha, double beta, double gamma) {
            mValue1 = alpha;
            mValue2 = beta;
            mValue3 = gamma;
            mCalls = mCalls.concat("gotOrientation");
        }

        @Override
        protected void gotOrientationAbsolute(double alpha, double beta, double gamma) {
            mValue1 = alpha;
            mValue2 = beta;
            mValue3 = gamma;
            mCalls = mCalls.concat("gotOrientationAbsolute");
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

    private static class MockSensorManager implements DeviceSensors.SensorManagerProxy {
        private int mNumRegistered = 0;
        private int mNumUnRegistered = 0;
        private boolean mRotationVectorAvailable = true;
        private boolean mGameRotationVectorAvailable = true;
        private boolean mAccelerometerAvailable = true;

        private MockSensorManager() {}

        public void setGameRotationVectorAvailable(boolean available) {
            mGameRotationVectorAvailable = available;
        }

        public void setRotationVectorAvailable(boolean available) {
            mRotationVectorAvailable = available;
        }

        public void setAccelerometerAvailable(boolean available) {
            mAccelerometerAvailable = available;
        }

        private boolean isSensorTypeAvailable(int sensorType) {
            switch (sensorType) {
                case Sensor.TYPE_ROTATION_VECTOR:
                    return mRotationVectorAvailable;
                case Sensor.TYPE_GAME_ROTATION_VECTOR:
                    return mGameRotationVectorAvailable;
                case Sensor.TYPE_ACCELEROMETER:
                    return mAccelerometerAvailable;
            }
            return true;
        }

        @Override
        public boolean registerListener(
                SensorEventListener listener, int sensorType, int rate, Handler handler) {
            if (isSensorTypeAvailable(sensorType)) {
                mNumRegistered++;
                return true;
            }
            return false;
        }

        @Override
        public void unregisterListener(SensorEventListener listener, int sensorType) {
            mNumUnRegistered++;
        }
    }
}
