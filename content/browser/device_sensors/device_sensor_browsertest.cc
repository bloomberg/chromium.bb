// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "device/generic_sensor/platform_sensor.h"
#include "device/generic_sensor/platform_sensor_provider.h"
#include "device/sensors/data_fetcher_shared_memory.h"
#include "device/sensors/device_sensor_service.h"
#include "device/sensors/public/cpp/device_motion_hardware_buffer.h"
#include "device/sensors/public/cpp/device_orientation_hardware_buffer.h"

namespace content {

namespace {

class FakeDataFetcher : public device::DataFetcherSharedMemory {
 public:
  FakeDataFetcher() : sensor_data_available_(true) {}
  ~FakeDataFetcher() override {}

  void SetOrientationStartedCallback(
      base::Closure orientation_started_callback) {
    orientation_started_callback_ = orientation_started_callback;
  }

  void SetOrientationStoppedCallback(
      base::Closure orientation_stopped_callback) {
    orientation_stopped_callback_ = orientation_stopped_callback;
  }

  void SetOrientationAbsoluteStartedCallback(
      base::Closure orientation_absolute_started_callback) {
    orientation_absolute_started_callback_ =
        orientation_absolute_started_callback;
  }

  void SetOrientationAbsoluteStoppedCallback(
      base::Closure orientation_absolute_stopped_callback) {
    orientation_absolute_stopped_callback_ =
        orientation_absolute_stopped_callback;
  }

  bool Start(device::ConsumerType consumer_type, void* buffer) override {
    EXPECT_TRUE(buffer);

    switch (consumer_type) {
      case device::CONSUMER_TYPE_ORIENTATION: {
        device::DeviceOrientationHardwareBuffer* orientation_buffer =
            static_cast<device::DeviceOrientationHardwareBuffer*>(buffer);
        if (sensor_data_available_)
          UpdateOrientation(orientation_buffer);
        SetOrientationBufferReady(orientation_buffer);
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                orientation_started_callback_);
      } break;
      case device::CONSUMER_TYPE_ORIENTATION_ABSOLUTE: {
        device::DeviceOrientationHardwareBuffer* orientation_buffer =
            static_cast<device::DeviceOrientationHardwareBuffer*>(buffer);
        if (sensor_data_available_)
          UpdateOrientationAbsolute(orientation_buffer);
        SetOrientationBufferReady(orientation_buffer);
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                orientation_absolute_started_callback_);
      } break;
      default:
        return false;
    }
    return true;
  }

  bool Stop(device::ConsumerType consumer_type) override {
    switch (consumer_type) {
      case device::CONSUMER_TYPE_ORIENTATION:
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                orientation_stopped_callback_);
        break;
      case device::CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                orientation_absolute_stopped_callback_);
        break;
      default:
        return false;
    }
    return true;
  }

  void Fetch(unsigned consumer_bitmask) override {
    FAIL() << "fetch should not be called";
  }

  FetcherType GetType() const override { return FETCHER_TYPE_DEFAULT; }

  void SetSensorDataAvailable(bool available) {
    sensor_data_available_ = available;
  }

  void SetOrientationBufferReady(
      device::DeviceOrientationHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.all_available_sensors_are_active = true;
    buffer->seqlock.WriteEnd();
  }

  void UpdateOrientation(device::DeviceOrientationHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.alpha = 1;
    buffer->data.has_alpha = true;
    buffer->data.beta = 2;
    buffer->data.has_beta = true;
    buffer->data.gamma = 3;
    buffer->data.has_gamma = true;
    buffer->data.all_available_sensors_are_active = true;
    buffer->seqlock.WriteEnd();
  }

  void UpdateOrientationAbsolute(
      device::DeviceOrientationHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.alpha = 4;
    buffer->data.has_alpha = true;
    buffer->data.beta = 5;
    buffer->data.has_beta = true;
    buffer->data.gamma = 6;
    buffer->data.has_gamma = true;
    buffer->data.absolute = true;
    buffer->data.all_available_sensors_are_active = true;
    buffer->seqlock.WriteEnd();
  }

  // The below callbacks should be run on the UI thread.
  base::Closure orientation_started_callback_;
  base::Closure orientation_absolute_started_callback_;
  base::Closure orientation_stopped_callback_;
  base::Closure orientation_absolute_stopped_callback_;
  bool sensor_data_available_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDataFetcher);
};

class FakeAccelerometer : public device::PlatformSensor {
 public:
  FakeAccelerometer(mojo::ScopedSharedBufferMapping mapping,
                    device::PlatformSensorProvider* provider)
      : PlatformSensor(device::mojom::SensorType::ACCELEROMETER,
                       std::move(mapping),
                       provider) {}

  device::mojom::ReportingMode GetReportingMode() override {
    return device::mojom::ReportingMode::ON_CHANGE;
  }

  bool StartSensor(
      const device::PlatformSensorConfiguration& configuration) override {
    device::SensorReading reading;
    reading.timestamp =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    reading.values[0] = 4;
    reading.values[1] = 5;
    reading.values[2] = 6;
    UpdateSensorReading(reading, true);
    return true;
  }

  void StopSensor() override {}

 protected:
  ~FakeAccelerometer() override = default;

  bool CheckSensorConfiguration(
      const device::PlatformSensorConfiguration& configuration) override {
    return true;
  }

  device::PlatformSensorConfiguration GetDefaultConfiguration() override {
    return device::PlatformSensorConfiguration(60 /* frequency */);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAccelerometer);
};

class FakeLinearAccelerationSensor : public device::PlatformSensor {
 public:
  FakeLinearAccelerationSensor(mojo::ScopedSharedBufferMapping mapping,
                               device::PlatformSensorProvider* provider)
      : PlatformSensor(device::mojom::SensorType::LINEAR_ACCELERATION,
                       std::move(mapping),
                       provider) {}

  device::mojom::ReportingMode GetReportingMode() override {
    return device::mojom::ReportingMode::CONTINUOUS;
  }

  bool StartSensor(
      const device::PlatformSensorConfiguration& configuration) override {
    device::SensorReading reading;
    reading.timestamp =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    reading.values[0] = 1;
    reading.values[1] = 2;
    reading.values[2] = 3;
    UpdateSensorReading(reading, true);
    return true;
  }

  void StopSensor() override {}

 protected:
  ~FakeLinearAccelerationSensor() override = default;

  bool CheckSensorConfiguration(
      const device::PlatformSensorConfiguration& configuration) override {
    return true;
  }

  device::PlatformSensorConfiguration GetDefaultConfiguration() override {
    return device::PlatformSensorConfiguration(60 /* frequency */);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeLinearAccelerationSensor);
};

class FakeGyroscope : public device::PlatformSensor {
 public:
  FakeGyroscope(mojo::ScopedSharedBufferMapping mapping,
                device::PlatformSensorProvider* provider)
      : PlatformSensor(device::mojom::SensorType::GYROSCOPE,
                       std::move(mapping),
                       provider) {}

  device::mojom::ReportingMode GetReportingMode() override {
    return device::mojom::ReportingMode::ON_CHANGE;
  }

  bool StartSensor(
      const device::PlatformSensorConfiguration& configuration) override {
    device::SensorReading reading;
    reading.timestamp =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    reading.values[0] = 7;
    reading.values[1] = 8;
    reading.values[2] = 9;
    UpdateSensorReading(reading, true);
    return true;
  }

  void StopSensor() override {}

 protected:
  ~FakeGyroscope() override = default;

  bool CheckSensorConfiguration(
      const device::PlatformSensorConfiguration& configuration) override {
    return true;
  }

  device::PlatformSensorConfiguration GetDefaultConfiguration() override {
    return device::PlatformSensorConfiguration(60 /* frequency */);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGyroscope);
};

class FakeSensorProvider : public device::PlatformSensorProvider {
 public:
  static FakeSensorProvider* GetInstance() {
    return base::Singleton<FakeSensorProvider, base::LeakySingletonTraits<
                                                   FakeSensorProvider>>::get();
  }
  FakeSensorProvider() {}
  ~FakeSensorProvider() override = default;

  void set_accelerometer_is_available(bool accelerometer_is_available) {
    accelerometer_is_available_ = accelerometer_is_available;
  }

  void set_linear_acceleration_sensor_is_available(
      bool linear_acceleration_sensor_is_available) {
    linear_acceleration_sensor_is_available_ =
        linear_acceleration_sensor_is_available;
  }

  void set_gyroscope_is_available(bool gyroscope_is_available) {
    gyroscope_is_available_ = gyroscope_is_available;
  }

 protected:
  void CreateSensorInternal(device::mojom::SensorType type,
                            mojo::ScopedSharedBufferMapping mapping,
                            const CreateSensorCallback& callback) override {
    // Create Sensors here.
    scoped_refptr<device::PlatformSensor> sensor;

    switch (type) {
      case device::mojom::SensorType::ACCELEROMETER:
        if (accelerometer_is_available_)
          sensor = new FakeAccelerometer(std::move(mapping), this);
        break;
      case device::mojom::SensorType::LINEAR_ACCELERATION:
        if (linear_acceleration_sensor_is_available_)
          sensor = new FakeLinearAccelerationSensor(std::move(mapping), this);
        break;
      case device::mojom::SensorType::GYROSCOPE:
        if (gyroscope_is_available_)
          sensor = new FakeGyroscope(std::move(mapping), this);
        break;
      default:
        NOTIMPLEMENTED();
    }

    callback.Run(std::move(sensor));
  }

  bool accelerometer_is_available_ = true;
  bool linear_acceleration_sensor_is_available_ = true;
  bool gyroscope_is_available_ = true;
};

class DeviceSensorBrowserTest : public ContentBrowserTest {
 public:
  DeviceSensorBrowserTest()
      : fetcher_(nullptr),
        io_loop_finished_event_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void SetUpOnMainThread() override {
    // Initialize the RunLoops now that the main thread has been created.
    orientation_started_runloop_.reset(new base::RunLoop());
    orientation_stopped_runloop_.reset(new base::RunLoop());
    orientation_absolute_started_runloop_.reset(new base::RunLoop());
    orientation_absolute_stopped_runloop_.reset(new base::RunLoop());
#if defined(OS_ANDROID)
    // On Android, the DeviceSensorService lives on the UI thread.
    SetUpFetcher();
#endif  // defined(OS_ANDROID)
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DeviceSensorBrowserTest::SetUpOnIOThread,
                   base::Unretained(this)));
    io_loop_finished_event_.Wait();
  }

  void SetUpFetcher() {
    fetcher_ = new FakeDataFetcher();
    fetcher_->SetOrientationStartedCallback(
        orientation_started_runloop_->QuitClosure());
    fetcher_->SetOrientationStoppedCallback(
        orientation_stopped_runloop_->QuitClosure());
    fetcher_->SetOrientationAbsoluteStartedCallback(
        orientation_absolute_started_runloop_->QuitClosure());
    fetcher_->SetOrientationAbsoluteStoppedCallback(
        orientation_absolute_stopped_runloop_->QuitClosure());
    device::DeviceSensorService::GetInstance()->SetDataFetcherForTesting(
        fetcher_);
  }

  void SetUpOnIOThread() {
#if !defined(OS_ANDROID)
    // On non-Android platforms, the DeviceSensorService lives on the IO thread.
    SetUpFetcher();
#endif  // !defined(OS_ANDROID)
    sensor_provider_ = FakeSensorProvider::GetInstance();
    device::PlatformSensorProvider::SetProviderForTesting(sensor_provider_);
    io_loop_finished_event_.Signal();
  }

  void TearDown() override {
    device::PlatformSensorProvider::SetProviderForTesting(nullptr);
  }

  void DelayAndQuit(base::TimeDelta delay) {
    base::PlatformThread::Sleep(delay);
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void WaitForAlertDialogAndQuitAfterDelay(base::TimeDelta delay) {
    ShellJavaScriptDialogManager* dialog_manager =
        static_cast<ShellJavaScriptDialogManager*>(
            shell()->GetJavaScriptDialogManager(shell()->web_contents()));

    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
    dialog_manager->set_dialog_request_callback(
        base::Bind(&DeviceSensorBrowserTest::DelayAndQuit,
                   base::Unretained(this), delay));
    runner->Run();
  }

  FakeDataFetcher* fetcher_;
  FakeSensorProvider* sensor_provider_;

  // NOTE: These can only be initialized once the main thread has been created
  // and so must be pointers instead of plain objects.
  std::unique_ptr<base::RunLoop> orientation_started_runloop_;
  std::unique_ptr<base::RunLoop> orientation_stopped_runloop_;
  std::unique_ptr<base::RunLoop> orientation_absolute_started_runloop_;
  std::unique_ptr<base::RunLoop> orientation_absolute_stopped_runloop_;

 private:
  base::WaitableEvent io_loop_finished_event_;
};

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationTest) {
  // The test page will register an event handler for orientation events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl("device_sensors", "device_orientation_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  orientation_started_runloop_->Run();
  orientation_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationAbsoluteTest) {
  // The test page will register an event handler for absolute orientation
  // events, expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url =
      GetTestUrl("device_sensors", "device_orientation_absolute_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  orientation_absolute_started_runloop_->Run();
  orientation_absolute_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, MotionTest) {
  // The test page will register an event handler for motion events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl("device_sensors", "device_motion_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationNullTest) {
  // The test page registers an event handler for orientation events and
  // expects to get an event with null values, because no sensor data can be
  // provided.
  fetcher_->SetSensorDataAvailable(false);
  GURL test_url =
      GetTestUrl("device_sensors", "device_orientation_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  orientation_started_runloop_->Run();
  orientation_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationAbsoluteNullTest) {
  // The test page registers an event handler for absolute orientation events
  // and expects to get an event with null values, because no sensor data can be
  // provided.
  fetcher_->SetSensorDataAvailable(false);
  GURL test_url = GetTestUrl("device_sensors",
                             "device_orientation_absolute_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  orientation_absolute_started_runloop_->Run();
  orientation_absolute_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, MotionNullTest) {
  // The test page registers an event handler for motion events and
  // expects to get an event with null values, because no sensor data can be
  // provided.
  sensor_provider_->set_accelerometer_is_available(false);
  sensor_provider_->set_linear_acceleration_sensor_is_available(false);
  sensor_provider_->set_gyroscope_is_available(false);
  GURL test_url = GetTestUrl("device_sensors", "device_motion_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest,
                       MotionOnlySomeSensorsAreAvailableTest) {
  // The test page registers an event handler for motion events and
  // expects to get an event with only the gyroscope and linear acceleration
  // sensor values, because no accelerometer values can be provided.
  sensor_provider_->set_accelerometer_is_available(false);
  GURL test_url =
      GetTestUrl("device_sensors",
                 "device_motion_only_some_sensors_are_available_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, NullTestWithAlert) {
  // The test page registers an event handlers for motion/orientation events and
  // expects to get events with null values. The test raises a modal alert
  // dialog with a delay to test that the one-off null-events still propagate to
  // window after the alert is dismissed and the callbacks are invoked which
  // eventually navigate to #pass.
  fetcher_->SetSensorDataAvailable(false);
  sensor_provider_->set_accelerometer_is_available(false);
  sensor_provider_->set_linear_acceleration_sensor_is_available(false);
  sensor_provider_->set_gyroscope_is_available(false);
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);

  GURL test_url =
      GetTestUrl("device_sensors", "device_sensors_null_test_with_alert.html");
  shell()->LoadURL(test_url);

  // TODO(timvolodine): investigate if it is possible to test this without
  // delay, crbug.com/360044.
  WaitForAlertDialogAndQuitAfterDelay(base::TimeDelta::FromMilliseconds(500));

  orientation_started_runloop_->Run();
  orientation_stopped_runloop_->Run();
  same_tab_observer.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
