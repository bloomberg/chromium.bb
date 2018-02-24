// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "device/base/synchronization/one_writer_seqlock.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace content {

namespace {

class FakeAmbientLightSensor : public device::mojom::Sensor {
 public:
  FakeAmbientLightSensor() {
    shared_buffer_handle_ = mojo::SharedBufferHandle::Create(
        sizeof(device::SensorReadingSharedBuffer) *
        static_cast<uint64_t>(device::mojom::SensorType::LAST));

    if (!shared_buffer_handle_.is_valid())
      return;

    // Create read/write mapping now, to ensure it is kept writable
    // after the region is sealed read-only on Android.
    shared_buffer_mapping_ = shared_buffer_handle_->MapAtOffset(
        device::mojom::SensorInitParams::kReadBufferSizeForTests,
        GetBufferOffset());
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
    if (!shared_buffer_mapping_.get())
      return;

    device::SensorReading reading;
    reading.als.timestamp =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    reading.als.value = 50;

    device::SensorReadingSharedBuffer* buffer =
        static_cast<device::SensorReadingSharedBuffer*>(
            shared_buffer_mapping_.get());
    auto& seqlock = buffer->seqlock.value();
    seqlock.WriteBegin();
    buffer->reading = reading;
    seqlock.WriteEnd();

    if (client_)
      client_->SensorReadingChanged();
  }

 private:
  mojo::ScopedSharedBufferHandle shared_buffer_handle_;
  mojo::ScopedSharedBufferMapping shared_buffer_mapping_;
  device::mojom::SensorClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(FakeAmbientLightSensor);
};

class FakeSensorProvider : public device::mojom::SensorProvider {
 public:
  FakeSensorProvider() = default;
  ~FakeSensorProvider() override = default;

  // device::mojom::sensorProvider implementation.
  void GetSensor(device::mojom::SensorType type,
                 GetSensorCallback callback) override {
    switch (type) {
      case device::mojom::SensorType::AMBIENT_LIGHT: {
        auto sensor = std::make_unique<FakeAmbientLightSensor>();

        auto init_params = device::mojom::SensorInitParams::New();
        init_params->client_request = sensor->GetClient();
        init_params->memory = sensor->GetSharedBufferHandle();
        init_params->buffer_offset = sensor->GetBufferOffset();
        init_params->default_configuration = sensor->GetDefaultConfiguration();
        init_params->maximum_frequency = sensor->GetMaximumSupportedFrequency();
        init_params->minimum_frequency = sensor->GetMinimumSupportedFrequency();

        mojo::MakeStrongBinding(std::move(sensor),
                                mojo::MakeRequest(&init_params->sensor));
        std::move(callback).Run(device::mojom::SensorCreationResult::SUCCESS,
                                std::move(init_params));
        break;
      }
      default:
        NOTIMPLEMENTED();
    }
  }

 private:
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
    https_embedded_test_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    // Serve both a.com and b.com (and any other domain).
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_embedded_test_server_->InitializeAndListen());
    content::SetupCrossSiteRedirector(https_embedded_test_server_.get());
    https_embedded_test_server_->ServeFilesFromSourceDirectory(
        "content/test/data/generic_sensor");
    https_embedded_test_server_->StartAcceptingConnections();

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&GenericSensorBrowserTest::SetBinderOnIOThread,
                       base::Unretained(this)));

    io_loop_finished_event_.Wait();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetBinderOnIOThread() {
    // Because Device Service also runs in this process(browser process), here
    // we can directly set our binder to intercept interface requests against
    // it.
    service_manager::ServiceContext::SetGlobalBinderForTesting(
        device::mojom::kServiceName, device::mojom::SensorProvider::Name_,
        base::BindRepeating(
            &GenericSensorBrowserTest::BindSensorProviderRequest,
            base::Unretained(this)));

    io_loop_finished_event_.Signal();
  }

  void BindSensorProviderRequest(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle handle,
      const service_manager::BindSourceInfo& source_info) {
    if (!sensor_provider_available_)
      return;

    if (!fake_sensor_provider_)
      fake_sensor_provider_ = std::make_unique<FakeSensorProvider>();

    sensor_provider_bindings_.AddBinding(
        fake_sensor_provider_.get(),
        device::mojom::SensorProviderRequest(std::move(handle)));
  }

  void set_sensor_provider_available(bool sensor_provider_available) {
    sensor_provider_available_ = sensor_provider_available;
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_embedded_test_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::WaitableEvent io_loop_finished_event_;
  bool sensor_provider_available_ = true;
  std::unique_ptr<FakeSensorProvider> fake_sensor_provider_;
  mojo::BindingSet<device::mojom::SensorProvider> sensor_provider_bindings_;

  DISALLOW_COPY_AND_ASSIGN(GenericSensorBrowserTest);
};

// Flakily crashes on Linux ASAN/TSAN bots.  https://crbug.com/789515
// Flakily times out on Windows bots.  https://crbug.com/809537
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_AmbientLightSensorTest DISABLED_AmbientLightSensorTest
#else
#define MAYBE_AmbientLightSensorTest AmbientLightSensorTest
#endif
IN_PROC_BROWSER_TEST_F(GenericSensorBrowserTest, MAYBE_AmbientLightSensorTest) {
  // The test page will create an AmbientLightSensor object in Javascript,
  // expects to get events with fake values then navigates to #pass.
  GURL test_url =
      GetTestUrl("generic_sensor", "ambient_light_sensor_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(GenericSensorBrowserTest,
                       AmbientLightSensorCrossOriginIframeTest) {
  // Main frame is on a.com, iframe is on b.com.
  GURL main_frame_url =
      https_embedded_test_server_->GetURL("a.com", "/cross_origin_iframe.html");
  GURL iframe_url = https_embedded_test_server_->GetURL(
      "b.com", "/ambient_light_sensor_cross_origin_iframe_test.html");

  // Wait for the main frame, subframe, and the #pass/#fail commits.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 3);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(),
                                  "cross_origin_iframe", iframe_url));

  navigation_observer.Wait();

  content::RenderFrameHost* iframe =
      ChildFrameAt(shell()->web_contents()->GetMainFrame(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ("pass", iframe->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(GenericSensorBrowserTest, SensorProviderUnavailable) {
  // The test page will create an AmbientLightSensor object in Javascript,
  // expects to get a sensor error then navigates to #pass.
  set_sensor_provider_available(false);
  GURL test_url = GetTestUrl("generic_sensor",
                             "ambient_light_sensor_unavailable_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
