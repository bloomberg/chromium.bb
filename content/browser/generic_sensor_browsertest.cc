// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "device/base/synchronization/one_writer_seqlock.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/device/public/interfaces/sensor.mojom.h"
#include "services/device/public/interfaces/sensor_provider.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace content {

namespace {

class FakeAmbientLightSensor : public device::mojom::Sensor {
 public:
  FakeAmbientLightSensor() {
    shared_buffer_handle_ = mojo::SharedBufferHandle::Create(
        sizeof(device::SensorReadingSharedBuffer) *
        static_cast<uint64_t>(device::mojom::SensorType::LAST));
  }

  ~FakeAmbientLightSensor() override = default;

  // device::mojom::Sensor implemenation:
  void AddConfiguration(
      const device::PlatformSensorConfiguration& configuration,
      AddConfigurationCallback callback) override {
    std::move(callback).Run(true);
    SensorReadingChanged();
  }

  void GetDefaultConfiguration(
      GetDefaultConfigurationCallback callback) override {
    std::move(callback).Run(GetDefaultConfiguration());
  }

  void RemoveConfiguration(
      const device::PlatformSensorConfiguration& configuration) override {}

  void Suspend() override {}
  void Resume() override {}
  void ConfigureReadingChangeNotifications(bool enabled) override {}

  device::PlatformSensorConfiguration GetDefaultConfiguration() {
    return device::PlatformSensorConfiguration(
        device::SensorTraits<
            device::mojom::SensorType::AMBIENT_LIGHT>::kDefaultFrequency);
  }

  device::mojom::ReportingMode GetReportingMode() {
    return device::mojom::ReportingMode::ON_CHANGE;
  }

  double GetMaximumSupportedFrequency() {
    return device::SensorTraits<
        device::mojom::SensorType::AMBIENT_LIGHT>::kMaxAllowedFrequency;
  }
  double GetMinimumSupportedFrequency() { return 1.0; }

  device::mojom::SensorClientRequest GetClient() {
    return mojo::MakeRequest(&client_);
  }

  mojo::ScopedSharedBufferHandle GetSharedBufferHandle() {
    return shared_buffer_handle_->Clone(
        mojo::SharedBufferHandle::AccessMode::READ_ONLY);
  }

  uint64_t GetBufferOffset() {
    return device::SensorReadingSharedBuffer::GetOffset(
        device::mojom::SensorType::AMBIENT_LIGHT);
  }

  void SensorReadingChanged() {
    if (!shared_buffer_handle_.is_valid())
      return;

    mojo::ScopedSharedBufferMapping shared_buffer =
        shared_buffer_handle_->MapAtOffset(
            device::mojom::SensorInitParams::kReadBufferSizeForTests,
            GetBufferOffset());

    device::SensorReading reading;
    reading.als.timestamp =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    reading.als.value = 50;

    device::SensorReadingSharedBuffer* buffer =
        static_cast<device::SensorReadingSharedBuffer*>(shared_buffer.get());
    auto& seqlock = buffer->seqlock.value();
    seqlock.WriteBegin();
    buffer->reading = reading;
    seqlock.WriteEnd();

    if (client_)
      client_->SensorReadingChanged();
  }

 private:
  mojo::ScopedSharedBufferHandle shared_buffer_handle_;
  device::mojom::SensorClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(FakeAmbientLightSensor);
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

  // device::mojom::sensorProvider implementation.
  void GetSensor(device::mojom::SensorType type,
                 device::mojom::SensorRequest sensor_request,
                 GetSensorCallback callback) override {
    switch (type) {
      case device::mojom::SensorType::AMBIENT_LIGHT: {
        auto sensor = base::MakeUnique<FakeAmbientLightSensor>();

        auto init_params = device::mojom::SensorInitParams::New();
        init_params->memory = sensor->GetSharedBufferHandle();
        init_params->buffer_offset = sensor->GetBufferOffset();
        init_params->default_configuration = sensor->GetDefaultConfiguration();
        init_params->maximum_frequency = sensor->GetMaximumSupportedFrequency();
        init_params->minimum_frequency = sensor->GetMinimumSupportedFrequency();

        std::move(callback).Run(std::move(init_params), sensor->GetClient());
        mojo::MakeStrongBinding(std::move(sensor), std::move(sensor_request));
        break;
      }
      default:
        NOTIMPLEMENTED();
    }
  }

 private:
  mojo::Binding<device::mojom::SensorProvider> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeSensorProvider);
};

class GenericSensorBrowserTest : public ContentBrowserTest {
 public:
  GenericSensorBrowserTest()
      : io_loop_finished_event_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {
    scoped_feature_list_.InitWithFeatures(
        {features::kGenericSensor, features::kGenericSensorExtraClasses}, {});
  }

  ~GenericSensorBrowserTest() override {}

  void SetUpOnMainThread() override {
    fake_sensor_provider_ = base::MakeUnique<FakeSensorProvider>();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&GenericSensorBrowserTest::SetBinderOnIOThread,
                       base::Unretained(this)));

    io_loop_finished_event_.Wait();
  }

  void SetBinderOnIOThread() {
    // Because Device Service also runs in this process(browser process), here
    // we can directly set our binder to intercept interface requests against
    // it.
    service_manager::ServiceContext::SetGlobalBinderForTesting(
        device::mojom::kServiceName, device::mojom::SensorProvider::Name_,
        base::Bind(&FakeSensorProvider::Bind,
                   base::Unretained(fake_sensor_provider_.get())));

    io_loop_finished_event_.Signal();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::WaitableEvent io_loop_finished_event_;
  std::unique_ptr<FakeSensorProvider> fake_sensor_provider_;

  DISALLOW_COPY_AND_ASSIGN(GenericSensorBrowserTest);
};

IN_PROC_BROWSER_TEST_F(GenericSensorBrowserTest, AmbientLightSensorTest) {
  // The test page will create an AmbientLightSensor object in Javascript,
  // expects to get events with fake values then navigates to #pass.
  GURL test_url =
      GetTestUrl("generic_sensor", "ambient_light_sensor_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
