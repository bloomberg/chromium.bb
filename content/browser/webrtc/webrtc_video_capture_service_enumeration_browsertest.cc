// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"

namespace content {

// The mediadevices.ondevicechange event is currently not supported on Android.
#if defined(OS_ANDROID)
#define MAYBE_AddingAndRemovingVirtualDeviceTriggersMediaElementOnDeviceChange \
  DISABLED_AddingAndRemovingVirtualDeviceTriggersMediaElementOnDeviceChange
#else
#define MAYBE_AddingAndRemovingVirtualDeviceTriggersMediaElementOnDeviceChange \
  AddingAndRemovingVirtualDeviceTriggersMediaElementOnDeviceChange
#endif

namespace {

static const char kVideoCaptureHtmlFile[] = "/media/video_capture_test.html";
static const char kEnumerateVideoCaptureDevicesAndVerify[] =
    "enumerateVideoCaptureDevicesAndVerifyCount(%d)";
static const char kRegisterForDeviceChangeEvent[] =
    "registerForDeviceChangeEvent()";
static const char kWaitForDeviceChangeEvent[] = "waitForDeviceChangeEvent()";
static const char kResetHasReceivedChangedEventFlag[] =
    "resetHasReceivedChangedEventFlag()";

}  // anonymous namespace

// Integration test that obtains a connection to the video capture service via
// the Browser process' service manager. It then registers and unregisters
// virtual devices at the service and checks in JavaScript that the list of
// enumerated devices changes correspondingly.
class WebRtcVideoCaptureServiceEnumerationBrowserTest
    : public ContentBrowserTest,
      public video_capture::mojom::DevicesChangedObserver {
 public:
  WebRtcVideoCaptureServiceEnumerationBrowserTest()
      : devices_changed_observer_binding_(this) {
    scoped_feature_list_.InitAndEnableFeature(features::kMojoVideoCapture);
  }

  ~WebRtcVideoCaptureServiceEnumerationBrowserTest() override {}

  void ConnectToService() {
    connector_->BindInterface(video_capture::mojom::kServiceName, &provider_);
    provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory_));
    video_capture::mojom::DevicesChangedObserverPtr observer;
    devices_changed_observer_binding_.Bind(mojo::MakeRequest(&observer));
    factory_->RegisterVirtualDevicesChangedObserver(
        std::move(observer),
        false /*raise_event_if_virtual_devices_already_present*/);
  }

  void AddVirtualDevice(const std::string& device_id) {
    media::VideoCaptureDeviceInfo info;
    info.descriptor.device_id = device_id;
    info.descriptor.set_display_name(device_id);
    info.descriptor.capture_api = media::VideoCaptureApi::VIRTUAL_DEVICE;

    base::RunLoop wait_loop;
    closure_to_be_called_on_devices_changed_ = wait_loop.QuitClosure();
    video_capture::mojom::TextureVirtualDevicePtr virtual_device;
    factory_->AddTextureVirtualDevice(info, mojo::MakeRequest(&virtual_device));
    virtual_devices_by_id_.insert(
        std::make_pair(device_id, std::move(virtual_device)));
    // Wait for confirmation from the service.
    wait_loop.Run();
  }

  void RemoveVirtualDevice(const std::string& device_id) {
    base::RunLoop wait_loop;
    closure_to_be_called_on_devices_changed_ = wait_loop.QuitClosure();
    virtual_devices_by_id_.erase(device_id);
    // Wait for confirmation from the service.
    wait_loop.Run();
  }

  void DisconnectFromService() {
    factory_ = nullptr;
    provider_ = nullptr;
  }

  void EnumerateDevicesInRendererAndVerifyDeviceCount(
      int expected_device_count) {
    const std::string javascript_to_execute = base::StringPrintf(
        kEnumerateVideoCaptureDevicesAndVerify, expected_device_count);
    std::string result;
    ASSERT_TRUE(
        ExecuteScriptAndExtractString(shell(), javascript_to_execute, &result));
    ASSERT_EQ("OK", result);
  }

  void RegisterForDeviceChangeEventInRenderer() {
    ASSERT_TRUE(ExecuteScript(shell(), kRegisterForDeviceChangeEvent));
  }

  void WaitForDeviceChangeEventInRenderer() {
    std::string result;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        shell(), kWaitForDeviceChangeEvent, &result));
    ASSERT_EQ("OK", result);
  }

  void ResetHasReceivedChangedEventFlag() {
    ASSERT_TRUE(ExecuteScript(shell(), kResetHasReceivedChangedEventFlag));
  }

  // Implementation of video_capture::mojom::DevicesChangedObserver:
  void OnDevicesChanged() override {
    if (closure_to_be_called_on_devices_changed_) {
      std::move(closure_to_be_called_on_devices_changed_).Run();
    }
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Note: We are not planning to actually use any fake device, but we want
    // to avoid enumerating or otherwise calling into real capture devices.
    command_line->AppendSwitchASCII(switches::kUseFakeDeviceForMediaStream,
                                    "device-count=0");
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
  }

  void Initialize() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();

    NavigateToURL(shell(),
                  GURL(embedded_test_server()->GetURL(kVideoCaptureHtmlFile)));

    auto* connection = content::ServiceManagerConnection::GetForProcess();
    ASSERT_TRUE(connection);
    auto* connector = connection->GetConnector();
    ASSERT_TRUE(connector);
    connector_ = connector->Clone();
  }

  std::unique_ptr<service_manager::Connector> connector_;
  std::map<std::string, video_capture::mojom::TextureVirtualDevicePtr>
      virtual_devices_by_id_;

 private:
  mojo::Binding<video_capture::mojom::DevicesChangedObserver>
      devices_changed_observer_binding_;
  base::test::ScopedFeatureList scoped_feature_list_;
  video_capture::mojom::DeviceFactoryProviderPtr provider_;
  video_capture::mojom::DeviceFactoryPtr factory_;
  base::OnceClosure closure_to_be_called_on_devices_changed_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcVideoCaptureServiceEnumerationBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebRtcVideoCaptureServiceEnumerationBrowserTest,
                       SingleAddedVirtualDeviceGetsEnumerated) {
  Initialize();
  ConnectToService();

  // Exercise
  AddVirtualDevice("test");
  EnumerateDevicesInRendererAndVerifyDeviceCount(1);

  // Tear down
  RemoveVirtualDevice("test");
  DisconnectFromService();
}

IN_PROC_BROWSER_TEST_F(WebRtcVideoCaptureServiceEnumerationBrowserTest,
                       RemoveVirtualDeviceAfterItHasBeenEnumerated) {
  Initialize();
  ConnectToService();

  AddVirtualDevice("test_1");
  AddVirtualDevice("test_2");
  EnumerateDevicesInRendererAndVerifyDeviceCount(2);
  RemoveVirtualDevice("test_1");
  EnumerateDevicesInRendererAndVerifyDeviceCount(1);
  RemoveVirtualDevice("test_2");
  EnumerateDevicesInRendererAndVerifyDeviceCount(0);

  // Tear down
  DisconnectFromService();
}

IN_PROC_BROWSER_TEST_F(
    WebRtcVideoCaptureServiceEnumerationBrowserTest,
    MAYBE_AddingAndRemovingVirtualDeviceTriggersMediaElementOnDeviceChange) {
  Initialize();
  ConnectToService();
  RegisterForDeviceChangeEventInRenderer();

  // Exercise
  AddVirtualDevice("test");

  WaitForDeviceChangeEventInRenderer();
  ResetHasReceivedChangedEventFlag();

  RemoveVirtualDevice("test");
  WaitForDeviceChangeEventInRenderer();

  // Tear down
  DisconnectFromService();
}

}  // namespace content
