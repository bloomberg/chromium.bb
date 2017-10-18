// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
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
#include "device/sensors/data_fetcher_shared_memory.h"
#include "device/sensors/device_sensor_service.h"
#include "device/sensors/public/cpp/device_motion_hardware_buffer.h"
#include "device/sensors/public/cpp/device_orientation_hardware_buffer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/device/public/interfaces/sensor.mojom.h"
#include "services/device/public/interfaces/sensor_provider.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"

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

class FakeSensor : public device::mojom::Sensor {
 public:
  FakeSensor(device::mojom::SensorType sensor_type)
      : sensor_type_(sensor_type) {
    shared_buffer_handle_ = mojo::SharedBufferHandle::Create(
        sizeof(device::SensorReadingSharedBuffer) *
        static_cast<uint64_t>(device::mojom::SensorType::LAST));
  }

  ~FakeSensor() override = default;

  // device::mojom::Sensor:
  void AddConfiguration(
      const device::PlatformSensorConfiguration& configuration,
      AddConfigurationCallback callback) override {
    std::move(callback).Run(true);
    SensorReadingChanged();
  }

  // device::mojom::Sensor:
  void GetDefaultConfiguration(
      GetDefaultConfigurationCallback callback) override {
    std::move(callback).Run(GetDefaultConfiguration());
  }

  // device::mojom::Sensor:
  void RemoveConfiguration(
      const device::PlatformSensorConfiguration& configuration) override {}

  // device::mojom::Sensor:
  void Suspend() override {}
  void Resume() override {}
  void ConfigureReadingChangeNotifications(bool enabled) override {
    reading_notification_enabled_ = enabled;
  }

  device::PlatformSensorConfiguration GetDefaultConfiguration() {
    return device::PlatformSensorConfiguration(60 /* frequency */);
  }

  device::mojom::ReportingMode GetReportingMode() {
    return device::mojom::ReportingMode::ON_CHANGE;
  }

  double GetMaximumSupportedFrequency() { return 60.0; }
  double GetMinimumSupportedFrequency() { return 1.0; }

  device::mojom::SensorClientRequest GetClient() {
    return mojo::MakeRequest(&client_);
  }

  mojo::ScopedSharedBufferHandle GetSharedBufferHandle() {
    return shared_buffer_handle_->Clone(
        mojo::SharedBufferHandle::AccessMode::READ_ONLY);
  }

  uint64_t GetBufferOffset() {
    return device::SensorReadingSharedBuffer::GetOffset(sensor_type_);
  }

  void set_reading(device::SensorReading reading) { reading_ = reading; }

  void SensorReadingChanged() {
    if (!shared_buffer_handle_.is_valid())
      return;

    mojo::ScopedSharedBufferMapping shared_buffer =
        shared_buffer_handle_->MapAtOffset(
            device::mojom::SensorInitParams::kReadBufferSizeForTests,
            GetBufferOffset());

    device::SensorReadingSharedBuffer* buffer =
        static_cast<device::SensorReadingSharedBuffer*>(shared_buffer.get());
    auto& seqlock = buffer->seqlock.value();
    seqlock.WriteBegin();
    buffer->reading = reading_;
    seqlock.WriteEnd();

    if (client_ && reading_notification_enabled_)
      client_->SensorReadingChanged();
  }

 private:
  device::mojom::SensorType sensor_type_;
  bool reading_notification_enabled_ = true;
  mojo::ScopedSharedBufferHandle shared_buffer_handle_;
  device::mojom::SensorClientPtr client_;
  device::SensorReading reading_;

  DISALLOW_COPY_AND_ASSIGN(FakeSensor);
};

class FakeSensorProvider : public device::mojom::SensorProvider {
 public:
  FakeSensorProvider() : binding_(this) {}
  ~FakeSensorProvider() override = default;

  void Bind(const std::string& interface_name,
            mojo::ScopedMessagePipeHandle handle,
            const service_manager::BindSourceInfo& source_info) {
    DCHECK(!binding_.is_bound());
    binding_.Bind(device::mojom::SensorProviderRequest(std::move(handle)));
  }

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

  // device::mojom::sensorProvider:
  void GetSensor(device::mojom::SensorType type,
                 device::mojom::SensorRequest sensor_request,
                 GetSensorCallback callback) override {
    std::unique_ptr<FakeSensor> sensor;
    device::SensorReading reading;
    reading.raw.timestamp =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();

    switch (type) {
      case device::mojom::SensorType::ACCELEROMETER:
        if (accelerometer_is_available_) {
          sensor = base::MakeUnique<FakeSensor>(
              device::mojom::SensorType::ACCELEROMETER);
          reading.accel.x = 4;
          reading.accel.y = 5;
          reading.accel.z = 6;
        }
        break;
      case device::mojom::SensorType::LINEAR_ACCELERATION:
        if (linear_acceleration_sensor_is_available_) {
          sensor = base::MakeUnique<FakeSensor>(
              device::mojom::SensorType::LINEAR_ACCELERATION);
          reading.accel.x = 1;
          reading.accel.y = 2;
          reading.accel.z = 3;
        }
        break;
      case device::mojom::SensorType::GYROSCOPE:
        if (gyroscope_is_available_) {
          sensor = base::MakeUnique<FakeSensor>(
              device::mojom::SensorType::GYROSCOPE);
          reading.gyro.x = 7;
          reading.gyro.y = 8;
          reading.gyro.z = 9;
        }
        break;
      default:
        NOTIMPLEMENTED();
    }

    if (sensor) {
      sensor->set_reading(reading);
      auto init_params = device::mojom::SensorInitParams::New();
      init_params->memory = sensor->GetSharedBufferHandle();
      init_params->buffer_offset = sensor->GetBufferOffset();
      init_params->default_configuration = sensor->GetDefaultConfiguration();
      init_params->maximum_frequency = sensor->GetMaximumSupportedFrequency();
      init_params->minimum_frequency = sensor->GetMinimumSupportedFrequency();

      std::move(callback).Run(std::move(init_params), sensor->GetClient());
      mojo::MakeStrongBinding(std::move(sensor), std::move(sensor_request));
    } else {
      std::move(callback).Run(nullptr, nullptr);
    }
  }

 private:
  mojo::Binding<device::mojom::SensorProvider> binding_;
  bool accelerometer_is_available_ = true;
  bool linear_acceleration_sensor_is_available_ = true;
  bool gyroscope_is_available_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeSensorProvider);
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
    sensor_provider_ = base::MakeUnique<FakeSensorProvider>();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&DeviceSensorBrowserTest::SetUpOnIOThread,
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
    // Because Device Service also runs in this process(browser process), here
    // we can directly set our binder to intercept interface requests against
    // it.
    service_manager::ServiceContext::SetGlobalBinderForTesting(
        device::mojom::kServiceName, device::mojom::SensorProvider::Name_,
        base::Bind(&FakeSensorProvider::Bind,
                   base::Unretained(sensor_provider_.get())));

    io_loop_finished_event_.Signal();
  }

  void DelayAndQuit(base::TimeDelta delay) {
    base::PlatformThread::Sleep(delay);
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
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
  std::unique_ptr<FakeSensorProvider> sensor_provider_;

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
