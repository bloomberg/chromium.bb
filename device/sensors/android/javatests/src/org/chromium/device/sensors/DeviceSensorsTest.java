// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.sensors;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.support.test.filters.SmallTest;
import android.test.AndroidTestCase;

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
        boolean start = mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        assertTrue(start);
        assertTrue("should contain all motion sensors",
                mDeviceSensors.mActiveSensors.containsAll(DeviceSensors.DEVICE_MOTION_SENSORS));
        assertTrue(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);

        assertEquals(DeviceSensors.DEVICE_MOTION_SENSORS.size(), mMockSensorManager.mNumRegistered);
        assertEquals(0, mMockSensorManager.mNumUnRegistered);
        assertEquals(DeviceSensors.DEVICE_MOTION_SENSORS.size(),
                mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    @SmallTest
    public void testRegisterSensorsDeviceOrientation() {
        boolean start = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);

        assertTrue(start);
        assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_A));
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
        assertTrue(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        assertEquals(OrientationSensorType.GAME_ROTATION_VECTOR,
                mDeviceSensors.getOrientationSensorTypeUsed());

        assertEquals(DeviceSensors.DEVICE_ORIENTATION_SENSORS_A.size(),
                mMockSensorManager.mNumRegistered);
        assertEquals(0, mMockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testRegisterSensorsDeviceOrientationGameRotationVectorNotAvailable() {
        MockSensorManager mockSensorManager = new MockSensorManager();
        mockSensorManager.setGameRotationVectorAvailable(false);
        mDeviceSensors.setSensorManagerProxy(mockSensorManager);
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);

        assertTrue(startOrientation);
        assertTrue(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        assertTrue("should contain option B orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_B));
        assertEquals(OrientationSensorType.ROTATION_VECTOR,
                mDeviceSensors.getOrientationSensorTypeUsed());

        assertEquals(DeviceSensors.DEVICE_ORIENTATION_SENSORS_B.size(),
                mockSensorManager.mNumRegistered);
        assertEquals(0, mockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testRegisterSensorsDeviceOrientationBothRotationVectorsNotAvailable() {
        MockSensorManager mockSensorManager = new MockSensorManager();
        mockSensorManager.setGameRotationVectorAvailable(false);
        mockSensorManager.setRotationVectorAvailable(false);
        mDeviceSensors.setSensorManagerProxy(mockSensorManager);
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);

        assertTrue(startOrientation);
        assertTrue(mDeviceSensors.mDeviceOrientationIsActive);
        assertTrue(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        assertTrue("should contain option C orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_C));
        assertEquals(OrientationSensorType.ACCELEROMETER_MAGNETIC,
                mDeviceSensors.getOrientationSensorTypeUsed());

        assertEquals(DeviceSensors.DEVICE_ORIENTATION_SENSORS_C.size(),
                mockSensorManager.mNumRegistered);
        assertEquals(0, mockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testRegisterSensorsDeviceOrientationNoSensorsAvailable() {
        MockSensorManager mockSensorManager = new MockSensorManager();
        mockSensorManager.setGameRotationVectorAvailable(false);
        mockSensorManager.setRotationVectorAvailable(false);
        mockSensorManager.setAccelerometerAvailable(false);
        mDeviceSensors.setSensorManagerProxy(mockSensorManager);
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);

        assertFalse(startOrientation);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        assertTrue(mDeviceSensors.mActiveSensors.isEmpty());
        assertEquals(
                OrientationSensorType.NOT_AVAILABLE, mDeviceSensors.getOrientationSensorTypeUsed());

        assertEquals(0, mockSensorManager.mNumRegistered);
        assertEquals(0, mockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testRegisterSensorsDeviceMotionAndOrientation() {
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);
        boolean startMotion = mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        assertTrue(startOrientation);
        assertTrue(startMotion);
        assertTrue("should contain all motion sensors",
                mDeviceSensors.mActiveSensors.containsAll(DeviceSensors.DEVICE_MOTION_SENSORS));
        assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_A));

        Set<Integer> union = new HashSet<Integer>(DeviceSensors.DEVICE_ORIENTATION_SENSORS_A);
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
    public void testRegisterSensorsDeviceOrientationAbsolute() {
        boolean start = mDeviceSensors.start(0, ConsumerType.ORIENTATION_ABSOLUTE, 100);

        assertTrue(start);
        assertTrue("should contain all absolute orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS));
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        assertTrue(mDeviceSensors.mDeviceOrientationAbsoluteIsActive);

        assertEquals(DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS.size(),
                mMockSensorManager.mNumRegistered);
        assertEquals(0, mMockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testUnregisterSensorsDeviceOrientationAbsolute() {
        mDeviceSensors.start(0, ConsumerType.ORIENTATION_ABSOLUTE, 100);
        mDeviceSensors.stop(ConsumerType.ORIENTATION_ABSOLUTE);

        assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
        assertEquals(DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS.size(),
                mMockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testRegisterSensorsDeviceOrientationAndOrientationAbsolute() {
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);
        boolean startOrientationAbsolute =
                mDeviceSensors.start(0, ConsumerType.ORIENTATION_ABSOLUTE, 100);

        assertTrue(startOrientation);
        assertTrue(startOrientationAbsolute);
        assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_A));
        assertTrue("should contain all absolute orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS));

        Set<Integer> union = new HashSet<Integer>(DeviceSensors.DEVICE_ORIENTATION_SENSORS_A);
        union.addAll(DeviceSensors.DEVICE_ORIENTATION_ABSOLUTE_SENSORS);

        assertEquals(union.size(), mDeviceSensors.mActiveSensors.size());
        assertTrue(mDeviceSensors.mDeviceOrientationIsActive);
        assertTrue(mDeviceSensors.mDeviceOrientationAbsoluteIsActive);
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
        assertEquals(union.size(), mMockSensorManager.mNumRegistered);
        assertEquals(0, mMockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testRegisterSensorsDeviceLight() {
        boolean start = mDeviceSensors.start(0, ConsumerType.LIGHT, 100);

        assertTrue(start);
        assertTrue(mDeviceSensors.mDeviceLightIsActive);
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);

        assertEquals(DeviceSensors.DEVICE_LIGHT_SENSORS.size(), mMockSensorManager.mNumRegistered);
        assertEquals(0, mMockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testUnregisterSensorsDeviceMotion() {
        mDeviceSensors.start(0, ConsumerType.MOTION, 100);
        mDeviceSensors.stop(ConsumerType.MOTION);

        assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
        assertEquals(
                DeviceSensors.DEVICE_MOTION_SENSORS.size(), mMockSensorManager.mNumUnRegistered);
        assertEquals(0, mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    @SmallTest
    public void testUnregisterSensorsDeviceOrientation() {
        mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);
        mDeviceSensors.stop(ConsumerType.ORIENTATION);

        assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActiveWithBackupSensors);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
        assertEquals(DeviceSensors.DEVICE_ORIENTATION_SENSORS_A.size(),
                mMockSensorManager.mNumUnRegistered);
    }

    @SmallTest
    public void testUnregisterSensorsDeviceMotionAndOrientation() {
        mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);
        mDeviceSensors.start(0, ConsumerType.MOTION, 100);
        mDeviceSensors.stop(ConsumerType.MOTION);

        assertTrue("should contain all orientation sensors",
                mDeviceSensors.mActiveSensors.containsAll(
                        DeviceSensors.DEVICE_ORIENTATION_SENSORS_A));

        Set<Integer> diff = new HashSet<Integer>(DeviceSensors.DEVICE_MOTION_SENSORS);
        diff.removeAll(DeviceSensors.DEVICE_ORIENTATION_SENSORS_A);

        assertEquals(diff.size(), mMockSensorManager.mNumUnRegistered);

        mDeviceSensors.stop(ConsumerType.ORIENTATION);

        assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        assertEquals(diff.size() + DeviceSensors.DEVICE_ORIENTATION_SENSORS_A.size(),
                mMockSensorManager.mNumUnRegistered);
        assertEquals(0, mDeviceSensors.getNumberActiveDeviceMotionSensors());
    }

    @SmallTest
    public void testUnregisterSensorsLight() {
        mDeviceSensors.start(0, ConsumerType.LIGHT, 100);
        mDeviceSensors.stop(ConsumerType.LIGHT);

        assertTrue("should contain no sensors", mDeviceSensors.mActiveSensors.isEmpty());
        assertFalse(mDeviceSensors.mDeviceMotionIsActive);
        assertFalse(mDeviceSensors.mDeviceOrientationIsActive);
        assertFalse(mDeviceSensors.mDeviceLightIsActive);
    }

    @SmallTest
    public void testSensorChangedGotLight() {
        boolean startLight = mDeviceSensors.start(0, ConsumerType.LIGHT, 100);

        assertTrue(startLight);
        assertTrue(mDeviceSensors.mDeviceLightIsActive);

        float[] values = {200};
        mDeviceSensors.sensorChanged(Sensor.TYPE_LIGHT, values);
        mDeviceSensors.verifyCalls("gotLight");
        mDeviceSensors.verifyValue(200);
    }

    /**
     * Helper method to trigger an orientation change using the given sensorType.
     */
    private void changeOrientation(int sensorType, boolean absolute, String expectedChange) {
        boolean startOrientation = mDeviceSensors.start(
                0, absolute ? ConsumerType.ORIENTATION_ABSOLUTE : ConsumerType.ORIENTATION, 100);

        assertTrue(startOrientation);
        assertTrue(absolute ? mDeviceSensors.mDeviceOrientationAbsoluteIsActive
                            : mDeviceSensors.mDeviceOrientationIsActive);

        float alpha = (float) Math.PI / 4;
        float[] values = {0, 0, (float) Math.sin(alpha / 2), (float) Math.cos(alpha / 2), -1};
        mDeviceSensors.sensorChanged(sensorType, values);

        mDeviceSensors.verifyCalls(expectedChange);
        if (!expectedChange.isEmpty()) {
            mDeviceSensors.verifyValuesEpsilon(Math.toDegrees(alpha), 0, 0);
        }
    }

    @SmallTest
    public void testSensorChangedGotOrientationViaRotationVector() {
        changeOrientation(Sensor.TYPE_ROTATION_VECTOR, false /* absolute */, "");
    }

    @SmallTest
    public void testSensorChangedGotOrientationViaGameRotationVector() {
        changeOrientation(Sensor.TYPE_GAME_ROTATION_VECTOR, false /* absolute */, "gotOrientation");
    }

    @SmallTest
    public void testSensorChangedGotOrientationAbsoluteViaRotationVector() {
        changeOrientation(
                Sensor.TYPE_ROTATION_VECTOR, true /* absolute */, "gotOrientationAbsolute");
    }

    @SmallTest
    public void testSensorChangedGotOrientationAbsoluteViaGameRotationVector() {
        changeOrientation(Sensor.TYPE_GAME_ROTATION_VECTOR, true /* absolute */, "");
    }

    @SmallTest
    public void testSensorChangedGotOrientationAndOrientationAbsolute() {
        changeOrientation(Sensor.TYPE_GAME_ROTATION_VECTOR, false /* absolute */, "gotOrientation");
        changeOrientation(Sensor.TYPE_ROTATION_VECTOR, true /* absolute */, "gotOrientation"
                        + "gotOrientationAbsolute");
    }

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

    @SmallTest
    public void testSensorChangedGotAccelerationIncludingGravity() {
        mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_ACCELEROMETER, values);
        mDeviceSensors.verifyCalls("gotAccelerationIncludingGravity");
        mDeviceSensors.verifyValues(1, 2, 3);
    }

    @SmallTest
    public void testSensorChangedGotAcceleration() {
        mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_LINEAR_ACCELERATION, values);
        mDeviceSensors.verifyCalls("gotAcceleration");
        mDeviceSensors.verifyValues(1, 2, 3);
    }

    @SmallTest
    public void testSensorChangedGotRotationRate() {
        mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        float[] values = {1, 2, 3};
        mDeviceSensors.sensorChanged(Sensor.TYPE_GYROSCOPE, values);
        mDeviceSensors.verifyCalls("gotRotationRate");
        mDeviceSensors.verifyValues(1, 2, 3);
    }

    @SmallTest
    public void testSensorChangedGotOrientationAndAcceleration() {
        boolean startOrientation = mDeviceSensors.start(0, ConsumerType.ORIENTATION, 100);
        boolean startMotion = mDeviceSensors.start(0, ConsumerType.MOTION, 100);

        assertTrue(startOrientation);
        assertTrue(startMotion);
        assertTrue(mDeviceSensors.mDeviceMotionIsActive);
        assertTrue(mDeviceSensors.mDeviceOrientationIsActive);

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
        double[] expectedAngles = {Math.PI / 4, Math.PI / 2, 0}; // favor yaw instead of roll

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
    private void verifyOrientationAngles(
            float[] gravity, float[] magnetic, double[] expectedAngles) {
        float[] r = new float[9];
        double[] values = new double[3];
        SensorManager.getRotationMatrix(r, null, gravity, magnetic);
        DeviceSensors.computeDeviceOrientationFromRotationMatrix(r, values);

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
