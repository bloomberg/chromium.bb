// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
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

namespace content {

namespace {

class FakeDataFetcher : public device::DataFetcherSharedMemory {
 public:
  FakeDataFetcher() : sensor_data_available_(true) {}
  ~FakeDataFetcher() override {}

  void SetMotionStartedCallback(base::Closure motion_started_callback) {
    motion_started_callback_ = motion_started_callback;
  }

  void SetMotionStoppedCallback(base::Closure motion_stopped_callback) {
    motion_stopped_callback_ = motion_stopped_callback;
  }

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
      case device::CONSUMER_TYPE_MOTION: {
        device::DeviceMotionHardwareBuffer* motion_buffer =
            static_cast<device::DeviceMotionHardwareBuffer*>(buffer);
        if (sensor_data_available_)
          UpdateMotion(motion_buffer);
        SetMotionBufferReady(motion_buffer);
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                motion_started_callback_);
      } break;
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
      case device::CONSUMER_TYPE_MOTION:
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                motion_stopped_callback_);
        break;
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

  void SetMotionBufferReady(device::DeviceMotionHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.all_available_sensors_are_active = true;
    buffer->seqlock.WriteEnd();
  }

  void SetOrientationBufferReady(
      device::DeviceOrientationHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.all_available_sensors_are_active = true;
    buffer->seqlock.WriteEnd();
  }

  void UpdateMotion(device::DeviceMotionHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.acceleration_x = 1;
    buffer->data.has_acceleration_x = true;
    buffer->data.acceleration_y = 2;
    buffer->data.has_acceleration_y = true;
    buffer->data.acceleration_z = 3;
    buffer->data.has_acceleration_z = true;

    buffer->data.acceleration_including_gravity_x = 4;
    buffer->data.has_acceleration_including_gravity_x = true;
    buffer->data.acceleration_including_gravity_y = 5;
    buffer->data.has_acceleration_including_gravity_y = true;
    buffer->data.acceleration_including_gravity_z = 6;
    buffer->data.has_acceleration_including_gravity_z = true;

    buffer->data.rotation_rate_alpha = 7;
    buffer->data.has_rotation_rate_alpha = true;
    buffer->data.rotation_rate_beta = 8;
    buffer->data.has_rotation_rate_beta = true;
    buffer->data.rotation_rate_gamma = 9;
    buffer->data.has_rotation_rate_gamma = true;

    buffer->data.interval = 100;
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
  base::Closure motion_started_callback_;
  base::Closure orientation_started_callback_;
  base::Closure orientation_absolute_started_callback_;
  base::Closure motion_stopped_callback_;
  base::Closure orientation_stopped_callback_;
  base::Closure orientation_absolute_stopped_callback_;
  bool sensor_data_available_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDataFetcher);
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
    motion_started_runloop_.reset(new base::RunLoop());
    motion_stopped_runloop_.reset(new base::RunLoop());
    orientation_started_runloop_.reset(new base::RunLoop());
    orientation_stopped_runloop_.reset(new base::RunLoop());
    orientation_absolute_started_runloop_.reset(new base::RunLoop());
    orientation_absolute_stopped_runloop_.reset(new base::RunLoop());
#if defined(OS_ANDROID)
    // On Android, the DeviceSensorService lives on the UI thread.
    SetUpFetcher();
#else
    // On all other platforms, the DeviceSensorService lives on the IO thread.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DeviceSensorBrowserTest::SetUpOnIOThread,
                   base::Unretained(this)));
    io_loop_finished_event_.Wait();
#endif
  }

  void SetUpFetcher() {
    fetcher_ = new FakeDataFetcher();
    fetcher_->SetMotionStartedCallback(motion_started_runloop_->QuitClosure());
    fetcher_->SetMotionStoppedCallback(motion_stopped_runloop_->QuitClosure());
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
    SetUpFetcher();
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

  // NOTE: These can only be initialized once the main thread has been created
  // and so must be pointers instead of plain objects.
  std::unique_ptr<base::RunLoop> motion_started_runloop_;
  std::unique_ptr<base::RunLoop> motion_stopped_runloop_;
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
  motion_started_runloop_->Run();
  motion_stopped_runloop_->Run();
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
  fetcher_->SetSensorDataAvailable(false);
  GURL test_url = GetTestUrl("device_sensors", "device_motion_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  motion_started_runloop_->Run();
  motion_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, NullTestWithAlert) {
  // The test page registers an event handlers for motion/orientation events and
  // expects to get events with null values. The test raises a modal alert
  // dialog with a delay to test that the one-off null-events still propagate to
  // window after the alert is dismissed and the callbacks are invoked which
  // eventually navigate to #pass.
  fetcher_->SetSensorDataAvailable(false);
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);

  GURL test_url =
      GetTestUrl("device_sensors", "device_sensors_null_test_with_alert.html");
  shell()->LoadURL(test_url);

  // TODO(timvolodine): investigate if it is possible to test this without
  // delay, crbug.com/360044.
  WaitForAlertDialogAndQuitAfterDelay(base::TimeDelta::FromMilliseconds(500));

  motion_started_runloop_->Run();
  motion_stopped_runloop_->Run();
  orientation_started_runloop_->Run();
  orientation_stopped_runloop_->Run();
  same_tab_observer.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
