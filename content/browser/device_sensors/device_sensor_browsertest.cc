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

  void set_relative_orientation_sensor_is_available(
      bool relative_orientation_sensor_is_available) {
    relative_orientation_sensor_is_available_ =
        relative_orientation_sensor_is_available;
  }

  void set_absolute_orientation_sensor_is_available(
      bool absolute_orientation_sensor_is_available) {
    absolute_orientation_sensor_is_available_ =
        absolute_orientation_sensor_is_available;
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
      case device::mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
        if (relative_orientation_sensor_is_available_) {
          sensor = base::MakeUnique<FakeSensor>(
              device::mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
          reading.orientation_euler.x = 2;  // beta
          reading.orientation_euler.y = 3;  // gamma
          reading.orientation_euler.z = 1;  // alpha
        }
        break;
      case device::mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
        if (absolute_orientation_sensor_is_available_) {
          sensor = base::MakeUnique<FakeSensor>(
              device::mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
          reading.orientation_euler.x = 5;  // beta
          reading.orientation_euler.y = 6;  // gamma
          reading.orientation_euler.z = 4;  // alpha
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
  bool relative_orientation_sensor_is_available_ = true;
  bool absolute_orientation_sensor_is_available_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeSensorProvider);
};

class DeviceSensorBrowserTest : public ContentBrowserTest {
 public:
  DeviceSensorBrowserTest()
      : io_loop_finished_event_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void SetUpOnMainThread() override {
    sensor_provider_ = base::MakeUnique<FakeSensorProvider>();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&DeviceSensorBrowserTest::SetUpOnIOThread,
                       base::Unretained(this)));
    io_loop_finished_event_.Wait();
  }

  void SetUpOnIOThread() {
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

  std::unique_ptr<FakeSensorProvider> sensor_provider_;

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
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest,
                       OrientationFallbackToAbsoluteTest) {
  // The test page will register an event handler for orientation events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  //
  // Here the relative orientation sensor is not available, but the absolute
  // orientation sensor is available, so orientation event will provide the
  // absolute orientation data.
  sensor_provider_->set_relative_orientation_sensor_is_available(false);
  sensor_provider_->set_absolute_orientation_sensor_is_available(true);
  GURL test_url = GetTestUrl(
      "device_sensors", "device_orientation_fallback_to_absolute_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationAbsoluteTest) {
  // The test page will register an event handler for absolute orientation
  // events, expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url =
      GetTestUrl("device_sensors", "device_orientation_absolute_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
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
  //
  // Here it needs to set both the relative and absolute orientation sensors
  // unavailable, since orientation event will fallback to absolute orientation
  // sensor if it is available.
  sensor_provider_->set_relative_orientation_sensor_is_available(false);
  sensor_provider_->set_absolute_orientation_sensor_is_available(false);
  GURL test_url =
      GetTestUrl("device_sensors", "device_orientation_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationAbsoluteNullTest) {
  // The test page registers an event handler for absolute orientation events
  // and expects to get an event with null values, because no sensor data can be
  // provided.
  sensor_provider_->set_absolute_orientation_sensor_is_available(false);
  GURL test_url = GetTestUrl("device_sensors",
                             "device_orientation_absolute_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
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
  sensor_provider_->set_relative_orientation_sensor_is_available(false);
  sensor_provider_->set_absolute_orientation_sensor_is_available(false);
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

  same_tab_observer.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
