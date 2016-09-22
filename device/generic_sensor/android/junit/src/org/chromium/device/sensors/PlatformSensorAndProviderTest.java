// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.sensors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.util.SparseArray;

import org.chromium.base.test.util.Feature;
import org.chromium.device.mojom.ReportingMode;
import org.chromium.device.mojom.SensorInitParams;
import org.chromium.device.mojom.SensorType;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for PlatformSensor and PlatformSensorProvider.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlatformSensorAndProviderTest {
    @Mock
    private Context mContext;
    @Mock
    private SensorManager mSensorManager;
    @Mock
    private PlatformSensorProvider mPlatformSensorProvider;
    private ByteBuffer mSharedBuffer;
    private final SparseArray<List<Sensor>> mMockSensors = new SparseArray<>();
    private static final long PLATFORM_SENSOR_ANDROID = 123456789L;
    private static final long PLATFORM_SENSOR_TIMESTAMP = 314159265358979L;
    private static final double MILLISECONDS_IN_NANOSECOND = 0.000001d;

    /**
     * Class that overrides thread management callbacks for testing purposes.
     */
    private static class TestPlatformSensorProvider extends PlatformSensorProvider {
        public TestPlatformSensorProvider(Context context) {
            super(context);
        }

        @Override
        public Handler getHandler() {
            return new Handler();
        }

        @Override
        protected void startSensorThread() {}

        @Override
        protected void stopSensorThread() {}
    }

    /**
     *  Class that overrides native callbacks for testing purposes.
     */
    private static class TestPlatformSensor extends PlatformSensor {
        public TestPlatformSensor(
                Sensor sensor, int readingCount, PlatformSensorProvider provider) {
            super(sensor, readingCount, provider);
        }

        @Override
        protected void sensorReadingChanged() {}
        @Override
        protected void sensorError() {}
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        // Remove all mock sensors before the test.
        mMockSensors.clear();
        doReturn(mSensorManager).when(mContext).getSystemService(Context.SENSOR_SERVICE);
        doAnswer(
                new Answer<List<Sensor>>() {
                    @Override
                    public List<Sensor> answer(final InvocationOnMock invocation)
                            throws Throwable {
                        return getMockSensors((int) (Integer) (invocation.getArguments())[0]);
                    }
                })
                .when(mSensorManager)
                .getSensorList(anyInt());
        doReturn(mSensorManager).when(mPlatformSensorProvider).getSensorManager();
        doReturn(new Handler()).when(mPlatformSensorProvider).getHandler();
        // By default, allow successful registration of SensorEventListeners.
        doReturn(true)
                .when(mSensorManager)
                .registerListener(any(SensorEventListener.class), any(Sensor.class), anyInt(),
                        any(Handler.class));
    }

    /**
     * Test that PlatformSensorProvider cannot create sensors if sensor manager is null.
     */
    @Test
    @Feature({"PlatformSensorProvider"})
    public void testNullSensorManager() {
        doReturn(null).when(mContext).getSystemService(Context.SENSOR_SERVICE);
        PlatformSensorProvider provider = PlatformSensorProvider.create(mContext);
        PlatformSensor sensor = provider.createSensor(SensorType.AMBIENT_LIGHT);
        assertNull(sensor);
    }

    /**
     * Test that PlatformSensorProvider cannot create sensors that are not supported.
     */
    @Test
    @Feature({"PlatformSensorProvider"})
    public void testSensorNotSupported() {
        PlatformSensorProvider provider = PlatformSensorProvider.create(mContext);
        PlatformSensor sensor = provider.createSensor(SensorType.AMBIENT_LIGHT);
        assertNull(sensor);
    }

    /**
     * Test that PlatformSensorProvider maps device::SensorType to android.hardware.Sensor.TYPE_*.
     */
    @Test
    @Feature({"PlatformSensorProvider"})
    public void testSensorTypeMappings() {
        PlatformSensorProvider provider = PlatformSensorProvider.create(mContext);
        provider.createSensor(SensorType.AMBIENT_LIGHT);
        verify(mSensorManager).getSensorList(Sensor.TYPE_LIGHT);
        provider.createSensor(SensorType.ACCELEROMETER);
        verify(mSensorManager).getSensorList(Sensor.TYPE_ACCELEROMETER);
        provider.createSensor(SensorType.LINEAR_ACCELERATION);
        verify(mSensorManager).getSensorList(Sensor.TYPE_LINEAR_ACCELERATION);
        provider.createSensor(SensorType.GYROSCOPE);
        verify(mSensorManager).getSensorList(Sensor.TYPE_GYROSCOPE);
        provider.createSensor(SensorType.MAGNETOMETER);
        verify(mSensorManager).getSensorList(Sensor.TYPE_MAGNETIC_FIELD);
    }

    /**
     * Test that PlatformSensorProvider can create sensors that are supported.
     */
    @Test
    @Feature({"PlatformSensorProvider"})
    public void testSensorSupported() {
        PlatformSensor sensor = createPlatformSensor(50000, Sensor.TYPE_LIGHT,
                SensorType.AMBIENT_LIGHT, Sensor.REPORTING_MODE_ON_CHANGE);
        assertNotNull(sensor);
    }

    /**
     * Test that PlatformSensor notifies PlatformSensorProvider when it starts (stops) polling,
     * and SensorEventListener is registered (unregistered) to sensor manager.
     */
    @Test
    @Feature({"PlatformSensor"})
    public void testSensorStartStop() {
        addMockSensor(50000, Sensor.TYPE_ACCELEROMETER, Sensor.REPORTING_MODE_CONTINUOUS);
        PlatformSensor sensor =
                PlatformSensor.create(Sensor.TYPE_ACCELEROMETER, 3, mPlatformSensorProvider);
        assertNotNull(sensor);

        sensor.startSensor(5);
        sensor.stopSensor();

        // Multiple start invocations.
        sensor.startSensor(1);
        sensor.startSensor(2);
        sensor.startSensor(3);
        // Same frequency, should not restart sensor
        sensor.startSensor(3);

        // Started polling with 5, 1, 2 and 3 Hz frequency.
        verify(mPlatformSensorProvider, times(4)).getHandler();
        verify(mPlatformSensorProvider, times(4)).sensorStarted(sensor);
        verify(mSensorManager, times(4))
                .registerListener(any(SensorEventListener.class), any(Sensor.class), anyInt(),
                        any(Handler.class));

        sensor.stopSensor();
        sensor.stopSensor();
        verify(mPlatformSensorProvider, times(3)).sensorStopped(sensor);
        verify(mSensorManager, times(4))
                .unregisterListener(any(SensorEventListener.class), any(Sensor.class));
    }

    /**
     * Test that PlatformSensorProvider is notified when PlatformSensor starts and in case of
     * failure, tells PlatformSensorProvider that the sensor is stopped, so that polling thread
     * can be stopped.
     */
    @Test
    @Feature({"PlatformSensor"})
    public void testSensorStartFails() {
        addMockSensor(50000, Sensor.TYPE_ACCELEROMETER, Sensor.REPORTING_MODE_CONTINUOUS);
        PlatformSensor sensor =
                PlatformSensor.create(Sensor.TYPE_ACCELEROMETER, 3, mPlatformSensorProvider);
        assertNotNull(sensor);

        doReturn(false)
                .when(mSensorManager)
                .registerListener(any(SensorEventListener.class), any(Sensor.class), anyInt(),
                        any(Handler.class));

        sensor.startSensor(5);
        verify(mPlatformSensorProvider, times(1)).sensorStarted(sensor);
        verify(mPlatformSensorProvider, times(1)).sensorStopped(sensor);
        verify(mPlatformSensorProvider, times(1)).getHandler();
    }

    /**
     * Test that PlatformSensor correctly checks supported configuration.
     */
    @Test
    @Feature({"PlatformSensor"})
    public void testSensorConfiguration() {
        // 5Hz min delay
        PlatformSensor sensor = createPlatformSensor(200000, Sensor.TYPE_ACCELEROMETER,
                SensorType.ACCELEROMETER, Sensor.REPORTING_MODE_CONTINUOUS);
        assertEquals(true, sensor.checkSensorConfiguration(5));
        assertEquals(false, sensor.checkSensorConfiguration(6));
    }

    /**
     * Test that PlatformSensor correctly returns its reporting mode.
     */
    @Test
    @Feature({"PlatformSensor"})
    public void testSensorOnChangeReportingMode() {
        PlatformSensor sensor = createPlatformSensor(50000, Sensor.TYPE_LIGHT,
                SensorType.AMBIENT_LIGHT, Sensor.REPORTING_MODE_ON_CHANGE);
        assertEquals(ReportingMode.ON_CHANGE, sensor.getReportingMode());
    }

    /**
     * Test that PlatformSensor doesn't notify client about sensor reading changes when
     * sensor reporting mode is ReportingMode.CONTINUOUS.
     */
    @Test
    @Feature({"PlatformSensor"})
    public void testSensorNotifierIsNotCalled() {
        TestPlatformSensor sensor = createTestPlatformSensor(
                50000, Sensor.TYPE_ACCELEROMETER, 3, Sensor.REPORTING_MODE_CONTINUOUS);
        initPlatformSensor(sensor, SensorInitParams.READ_BUFFER_SIZE);
        TestPlatformSensor spySensor = spy(sensor);
        SensorEvent event = createFakeEvent(3);
        assertNotNull(event);
        spySensor.onSensorChanged(event);
        verify(spySensor, never()).sensorReadingChanged();
    }

    /**
     * Test that shared buffer is correctly populated from SensorEvent.
     */
    @Test
    @Feature({"PlatformSensor"})
    public void testSensorBufferFromEvent() {
        TestPlatformSensor sensor = createTestPlatformSensor(
                50000, Sensor.TYPE_LIGHT, 1, Sensor.REPORTING_MODE_ON_CHANGE);
        initPlatformSensor(sensor, SensorInitParams.READ_BUFFER_SIZE);
        TestPlatformSensor spySensor = spy(sensor);
        SensorEvent event = createFakeEvent(1);
        assertNotNull(event);
        spySensor.onSensorChanged(event);
        verify(spySensor, times(1)).sensorReadingChanged();
        // Verify that timestamp correctly stored in buffer.
        assertEquals(MILLISECONDS_IN_NANOSECOND * PLATFORM_SENSOR_TIMESTAMP,
                mSharedBuffer.getDouble(), MILLISECONDS_IN_NANOSECOND);
        // Verify illuminance value is correctly stored in buffer and precision is not lost.
        assertEquals(1.0d + MILLISECONDS_IN_NANOSECOND, mSharedBuffer.getDouble(),
                MILLISECONDS_IN_NANOSECOND);
    }

    /**
     * Test that PlatformSensor notifies client when there is an error.
     */
    @Test
    @Feature({"PlatformSensor"})
    public void testSensorInvalidReadingSize() {
        TestPlatformSensor sensor = createTestPlatformSensor(
                50000, Sensor.TYPE_ACCELEROMETER, 3, Sensor.REPORTING_MODE_CONTINUOUS);
        initPlatformSensor(sensor, SensorInitParams.READ_BUFFER_SIZE);
        TestPlatformSensor spySensor = spy(sensor);
        // Accelerometer requires 3 reading values x,y and z, create fake event with 1 reading
        // value.
        SensorEvent event = createFakeEvent(1);
        assertNotNull(event);
        spySensor.onSensorChanged(event);
        verify(spySensor, times(1)).sensorError();
    }

    /**
     * Test that PlatformSensor notifies client when there is an error.
     */
    @Test
    @Feature({"PlatformSensor"})
    public void testSensorInvalidBufferSize() {
        TestPlatformSensor sensor = createTestPlatformSensor(
                50000, Sensor.TYPE_ACCELEROMETER, 3, Sensor.REPORTING_MODE_CONTINUOUS);
        // Create buffer that doesn't have enough capacity to hold sensor reading values.
        initPlatformSensor(sensor, 2);
        TestPlatformSensor spySensor = spy(sensor);
        SensorEvent event = createFakeEvent(3);
        assertNotNull(event);
        spySensor.onSensorChanged(event);
        verify(spySensor, times(1)).sensorError();
    }

    /**
     * Test that multiple PlatformSensor instances correctly register (unregister) to
     * sensor manager and notify PlatformSensorProvider when they start (stop) polling for data.
     */
    @Test
    @Feature({"PlatformSensor"})
    public void testMultipleSensorTypeInstances() {
        addMockSensor(200000, Sensor.TYPE_LIGHT, Sensor.REPORTING_MODE_ON_CHANGE);
        addMockSensor(50000, Sensor.TYPE_ACCELEROMETER, Sensor.REPORTING_MODE_CONTINUOUS);

        TestPlatformSensorProvider spyProvider = spy(new TestPlatformSensorProvider(mContext));
        PlatformSensor lightSensor = PlatformSensor.create(Sensor.TYPE_LIGHT, 1, spyProvider);
        assertNotNull(lightSensor);

        PlatformSensor accelerometerSensor =
                PlatformSensor.create(Sensor.TYPE_ACCELEROMETER, 3, spyProvider);
        assertNotNull(accelerometerSensor);

        lightSensor.startSensor(3);
        accelerometerSensor.startSensor(10);
        lightSensor.stopSensor();
        accelerometerSensor.stopSensor();

        verify(spyProvider, times(2)).getHandler();
        verify(spyProvider, times(1)).sensorStarted(lightSensor);
        verify(spyProvider, times(1)).sensorStarted(accelerometerSensor);
        verify(spyProvider, times(1)).sensorStopped(lightSensor);
        verify(spyProvider, times(1)).sensorStopped(accelerometerSensor);
        verify(spyProvider, times(1)).startSensorThread();
        verify(spyProvider, times(1)).stopSensorThread();
        verify(mSensorManager, times(2))
                .registerListener(any(SensorEventListener.class), any(Sensor.class), anyInt(),
                        any(Handler.class));
        verify(mSensorManager, times(2))
                .unregisterListener(any(SensorEventListener.class), any(Sensor.class));
    }

    /**
     * Creates fake event. The SensorEvent constructor is not accessible outside android.hardware
     * package, therefore, java reflection is used to make constructor accessible to construct
     * SensorEvent instance.
     */
    private SensorEvent createFakeEvent(int readingValuesNum) {
        try {
            Constructor<SensorEvent> sensorEventConstructor =
                    SensorEvent.class.getDeclaredConstructor(Integer.TYPE);
            sensorEventConstructor.setAccessible(true);
            SensorEvent event = sensorEventConstructor.newInstance(readingValuesNum);
            event.timestamp = PLATFORM_SENSOR_TIMESTAMP;
            for (int i = 0; i < readingValuesNum; ++i) {
                event.values[i] = (float) (i + 1.0f + MILLISECONDS_IN_NANOSECOND);
            }
            return event;
        } catch (InvocationTargetException | NoSuchMethodException | InstantiationException
                | IllegalAccessException e) {
            return null;
        }
    }

    private void initPlatformSensor(PlatformSensor sensor, long readingSize) {
        mSharedBuffer = ByteBuffer.allocate((int) readingSize);
        mSharedBuffer.order(ByteOrder.nativeOrder());
        sensor.initPlatformSensorAndroid(PLATFORM_SENSOR_ANDROID, mSharedBuffer);
    }

    private void addMockSensor(long minDelayUsec, int sensorType, int reportingMode) {
        List<Sensor> mockSensorList = new ArrayList<Sensor>();
        mockSensorList.add(createMockSensor(minDelayUsec, sensorType, reportingMode));
        mMockSensors.put(sensorType, mockSensorList);
    }

    private Sensor createMockSensor(long minDelayUsec, int sensorType, int reportingMode) {
        Sensor mockSensor = mock(Sensor.class);
        doReturn((int) minDelayUsec).when(mockSensor).getMinDelay();
        doReturn(reportingMode).when(mockSensor).getReportingMode();
        doReturn(sensorType).when(mockSensor).getType();
        return mockSensor;
    }

    private List<Sensor> getMockSensors(int sensorType) {
        if (mMockSensors.indexOfKey(sensorType) >= 0) {
            return mMockSensors.get(sensorType);
        }
        return new ArrayList<Sensor>();
    }

    private PlatformSensor createPlatformSensor(
            long minDelayUsec, int androidSensorType, int mojoSensorType, int reportingMode) {
        addMockSensor(minDelayUsec, androidSensorType, reportingMode);
        PlatformSensorProvider provider = PlatformSensorProvider.create(mContext);
        return provider.createSensor(mojoSensorType);
    }

    private TestPlatformSensor createTestPlatformSensor(
            long minDelayUsec, int androidSensorType, int readingCount, int reportingMode) {
        return new TestPlatformSensor(
                createMockSensor(minDelayUsec, androidSensorType, reportingMode), readingCount,
                mPlatformSensorProvider);
    }
}
