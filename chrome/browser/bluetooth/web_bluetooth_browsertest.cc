// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains browsertests for Web Bluetooth that depend on behavior
// defined in chrome/, not just in content/.

#include "base/callback.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_context_base.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"

namespace {

constexpr char kDeviceAddress[] = "00:00:00:00:00:00";
constexpr char kHeartRateUUIDString[] = "0000180d-0000-1000-8000-00805f9b34fb";
const device::BluetoothUUID kHeartRateUUID(kHeartRateUUIDString);

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  FakeBluetoothAdapter() = default;

  // Move-only class
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  void SetIsPresent(bool is_present) { is_present_ = is_present; }

  void SimulateDeviceAdvertisementReceived(
      const std::string& device_address) const {
    for (auto& observer : observers_) {
      observer.DeviceAdvertisementReceived(
          device_address, /*device_name=*/base::nullopt,
          /*advertisement_name=*/base::nullopt,
          /*rssi=*/base::nullopt, /*tx_power=*/base::nullopt,
          /*appearance=*/base::nullopt,
          /*advertised_uuids=*/{}, /*service_data_map=*/{},
          /*manufacturer_data_map=*/{});
    }
  }

  // device::BluetoothAdapter implementation:
  void AddObserver(device::BluetoothAdapter::Observer* observer) override {
    device::BluetoothAdapter::AddObserver(observer);
  }

  bool IsPresent() const override { return is_present_; }

  bool IsPowered() const override { return true; }

  device::BluetoothAdapter::ConstDeviceList GetDevices() const override {
    device::BluetoothAdapter::ConstDeviceList devices;
    for (const auto& it : mock_devices_)
      devices.push_back(it.get());
    return devices;
  }

  device::BluetoothDevice* GetDevice(const std::string& address) override {
    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address)
        return it.get();
    }
    return nullptr;
  }

  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> filter,
      base::OnceCallback<void(/*is_error*/ bool,
                              device::UMABluetoothDiscoverySessionOutcome)>
          callback) override {
    std::move(callback).Run(
        /*is_error=*/false,
        device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }

 protected:
  ~FakeBluetoothAdapter() override = default;

  bool is_present_ = true;
};

class FakeBluetoothGattService
    : public testing::NiceMock<device::MockBluetoothGattService> {
 public:
  FakeBluetoothGattService(device::MockBluetoothDevice* device,
                           const std::string& identifier,
                           const device::BluetoothUUID& uuid)
      : testing::NiceMock<device::MockBluetoothGattService>(device,
                                                            identifier,
                                                            uuid,
                                                            /*is_primary=*/true,
                                                            /*is_local=*/true) {
  }

  // Move-only class
  FakeBluetoothGattService(const FakeBluetoothGattService&) = delete;
  FakeBluetoothGattService operator=(const FakeBluetoothGattService&) = delete;
};

class FakeBluetoothGattConnection
    : public testing::NiceMock<device::MockBluetoothGattConnection> {
 public:
  FakeBluetoothGattConnection(scoped_refptr<device::BluetoothAdapter> adapter,
                              const std::string& device_address)
      : testing::NiceMock<device::MockBluetoothGattConnection>(adapter,
                                                               device_address) {
  }

  // Move-only class
  FakeBluetoothGattConnection(const FakeBluetoothGattConnection&) = delete;
  FakeBluetoothGattConnection operator=(const FakeBluetoothGattConnection&) =
      delete;
};

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  FakeBluetoothDevice(device::MockBluetoothAdapter* adapter,
                      const std::string& address)
      : testing::NiceMock<device::MockBluetoothDevice>(adapter,
                                                       /*bluetooth_class=*/0u,
                                                       /*name=*/"Test Device",
                                                       address,
                                                       /*paired=*/true,
                                                       /*connected=*/true) {}

  void CreateGattConnection(
      base::OnceCallback<void(std::unique_ptr<device::BluetoothGattConnection>)>
          callback,
      base::OnceCallback<void(enum ConnectErrorCode)> error_callback,
      base::Optional<device::BluetoothUUID> service_uuid =
          base::nullopt) override {
    SetConnected(true);
    gatt_services_discovery_complete_ = true;
    std::move(callback).Run(
        std::make_unique<FakeBluetoothGattConnection>(adapter_, GetAddress()));
  }

  bool IsGattServicesDiscoveryComplete() const override {
    return gatt_services_discovery_complete_;
  }

  std::vector<device::BluetoothRemoteGattService*> GetGattServices()
      const override {
    return GetMockServices();
  }

  // Move-only class
  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;

 protected:
  bool gatt_services_discovery_complete_ = false;
};

class FakeBluetoothChooser : public content::BluetoothChooser {
 public:
  FakeBluetoothChooser(content::BluetoothChooser::EventHandler event_handler,
                       const base::Optional<std::string>& device_to_select)
      : event_handler_(event_handler), device_to_select_(device_to_select) {}
  ~FakeBluetoothChooser() override = default;

  // content::BluetoothChooser implementation:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override {
    // Select the first device that is added if |device_to_select_| is not
    // populated.
    if (!device_to_select_) {
      event_handler_.Run(content::BluetoothChooser::Event::SELECTED, device_id);
      return;
    }

    // Otherwise, select the added device if its device ID matches
    // |device_to_select_|.
    if (device_to_select_.value() == device_id) {
      event_handler_.Run(content::BluetoothChooser::Event::SELECTED, device_id);
    }
  }

  // Move-only class
  FakeBluetoothChooser(const FakeBluetoothChooser&) = delete;
  FakeBluetoothChooser& operator=(const FakeBluetoothChooser&) = delete;

 private:
  content::BluetoothChooser::EventHandler event_handler_;
  base::Optional<std::string> device_to_select_;
};

class FakeBluetoothScanningPrompt : public content::BluetoothScanningPrompt {
 public:
  explicit FakeBluetoothScanningPrompt(
      const content::BluetoothScanningPrompt::EventHandler& event_handler)
      : event_handler_(event_handler) {}
  ~FakeBluetoothScanningPrompt() override = default;

  // Move-only class
  FakeBluetoothScanningPrompt(const FakeBluetoothScanningPrompt&) = delete;
  FakeBluetoothScanningPrompt& operator=(const FakeBluetoothScanningPrompt&) =
      delete;

  void RunPromptEventHandler(content::BluetoothScanningPrompt::Event event) {
    if (event_handler_.is_null()) {
      FAIL() << "event_handler_ is not set";
      return;
    }
    event_handler_.Run(event);
  }

 protected:
  content::BluetoothScanningPrompt::EventHandler event_handler_;
};

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  void SetDeviceToSelect(const std::string& device_address) {
    device_to_select_ = device_address;
  }

  // This method waits until ShowBluetoothScanningPrompt() has been called and
  // |scanning_prompt_| contains a pointer to the created prompt, so the test
  // will timeout if |navigator.bluetooth.requestLEScan()| has not been called
  // in JavaScript.
  void RunPromptEventHandler(content::BluetoothScanningPrompt::Event event) {
    if (!scanning_prompt_)
      scanning_prompt_creation_loop_.Run();
    scanning_prompt_->RunPromptEventHandler(event);
  }

 protected:
  // content::WebContentsDelegate implementation:
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override {
    return std::make_unique<FakeBluetoothChooser>(event_handler,
                                                  device_to_select_);
  }

  std::unique_ptr<content::BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler)
      override {
    auto scanning_prompt =
        std::make_unique<FakeBluetoothScanningPrompt>(event_handler);
    scanning_prompt_ = scanning_prompt.get();
    scanning_prompt_creation_loop_.Quit();
    return scanning_prompt;
  }

  base::Optional<std::string> device_to_select_;
  FakeBluetoothScanningPrompt* scanning_prompt_;

  // This RunLoop is used to ensure that |scanning_prompt_| is not nullptr when
  // RunPromptEventHandler() is called.
  base::RunLoop scanning_prompt_creation_loop_;
};

class WebBluetoothTest : public InProcessBrowserTest {
 public:
  WebBluetoothTest() = default;
  ~WebBluetoothTest() override = default;

  // Move-only class
  WebBluetoothTest(const WebBluetoothTest&) = delete;
  WebBluetoothTest& operator=(const WebBluetoothTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(juncai): Remove this switch once Web Bluetooth is supported on Linux
    // and Windows.
    // https://crbug.com/570344
    // https://crbug.com/507419
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    // Web Bluetooth permissions are granted for an origin. The tests for Web
    // Bluetooth permissions run code across a browser restart by splitting the
    // tests into separate test cases where the test prefixed with PRE_ runs
    // first. EmbeddedTestServer is not capable of maintaining a consistent
    // origin across the separate tests, so URLLoaderInterceptor is used instead
    // to intercept navigation requests and serve the test page. This enables
    // the separate test cases to grant and check permissions for the same
    // origin.
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url.host() == "example.com") {
                content::URLLoaderInterceptor::WriteResponse(
                    "content/test/data/simple_page.html", params->client.get());
                return true;
              }
              return false;
            }));
    ui_test_utils::NavigateToURL(browser(), GURL("https://example.com"));
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_THAT(
        web_contents_->GetMainFrame()->GetLastCommittedOrigin().Serialize(),
        testing::StartsWith("https://example.com"));

    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    global_values_ =
        device::BluetoothAdapterFactory::Get()->InitGlobalValuesForTesting();
    global_values_->SetLESupported(true);
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void AddFakeDevice(const std::string& device_address) {
    auto fake_device =
        std::make_unique<FakeBluetoothDevice>(adapter_.get(), device_address);
    fake_device->AddUUID(kHeartRateUUID);
    fake_device->AddMockService(std::make_unique<FakeBluetoothGattService>(
        fake_device.get(), kHeartRateUUIDString, kHeartRateUUID));
    adapter_->AddMockDevice(std::move(fake_device));
  }

  void RemoveFakeDevice(const std::string& device_address) {
    adapter_->RemoveMockDevice(device_address);
  }

  void SimulateDeviceAdvertisement(const std::string& device_address) {
    adapter_->SimulateDeviceAdvertisementReceived(device_address);
  }

  TestWebContentsDelegate* UseAndGetTestWebContentsDelegate() {
    if (!test_delegate_)
      test_delegate_ = std::make_unique<TestWebContentsDelegate>();
    web_contents_->SetDelegate(test_delegate_.get());
    return test_delegate_.get();
  }

  void SetDeviceToSelect(const std::string& device_address) {
    test_delegate_->SetDeviceToSelect(device_address);
  }

  std::unique_ptr<device::BluetoothAdapterFactory::GlobalValuesForTesting>
      global_values_;
  scoped_refptr<FakeBluetoothAdapter> adapter_;

  content::WebContents* web_contents_ = nullptr;
  std::unique_ptr<TestWebContentsDelegate> test_delegate_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, DISABLED_WebBluetoothAfterCrash) {
  // Make sure we can use Web Bluetooth after the tab crashes.
  // Set up adapter with one device.
  adapter_->SetIsPresent(false);
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{services: [0x180d]}]})"
      "  .catch(e => domAutomationController.send(e.toString()));",
      &result));
  EXPECT_EQ("NotFoundError: Bluetooth adapter not available.", result);

  // Crash the renderer process.
  content::RenderProcessHost* process =
      web_contents_->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // Reload tab.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Use Web Bluetooth again.
  std::string result_after_crash;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{services: [0x180d]}]})"
      "  .catch(e => domAutomationController.send(e.toString()));",
      &result_after_crash));
  EXPECT_EQ("NotFoundError: Bluetooth adapter not available.",
            result_after_crash);
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, KillSwitchShouldBlock) {
  // Turn on the global kill switch.
  std::map<std::string, std::string> params;
  params["Bluetooth"] =
      permissions::PermissionContextBase::kPermissionsKillSwitchBlockedValue;
  variations::AssociateVariationParams(
      permissions::PermissionContextBase::kPermissionsKillSwitchFieldStudy,
      "TestGroup", params);
  base::FieldTrialList::CreateFieldTrial(
      permissions::PermissionContextBase::kPermissionsKillSwitchFieldStudy,
      "TestGroup");

  std::string rejection;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{name: 'Hello'}]})"
      "  .then(() => { domAutomationController.send('Success'); },"
      "        reason => {"
      "      domAutomationController.send(reason.name + ': ' + reason.message);"
      "  });",
      &rejection));
  EXPECT_THAT(rejection,
              testing::MatchesRegex("NotFoundError: .*globally disabled.*"));
}

// Tests that using Finch field trial parameters for blocklist additions has
// the effect of rejecting requestDevice calls.
IN_PROC_BROWSER_TEST_F(WebBluetoothTest, BlocklistShouldBlock) {
  if (base::FieldTrialList::TrialExists("WebBluetoothBlocklist")) {
    LOG(INFO) << "WebBluetoothBlocklist field trial already configured.";
    ASSERT_NE(variations::GetVariationParamValue("WebBluetoothBlocklist",
                                                 "blocklist_additions")
                  .find("ed5f25a4"),
              std::string::npos)
        << "ERROR: WebBluetoothBlocklist field trial being tested in\n"
           "testing/variations/fieldtrial_testing_config_*.json must\n"
           "include this test's random UUID 'ed5f25a4' in\n"
           "blocklist_additions.\n";
  } else {
    LOG(INFO) << "Creating WebBluetoothBlocklist field trial for test.";
    // Create a field trial with test parameter.
    std::map<std::string, std::string> params;
    params["blocklist_additions"] = "ed5f25a4:e";
    variations::AssociateVariationParams("WebBluetoothBlocklist", "TestGroup",
                                         params);
    base::FieldTrialList::CreateFieldTrial("WebBluetoothBlocklist",
                                           "TestGroup");
  }

  std::string rejection;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{services: [0xed5f25a4]}]})"
      "  .then(() => { domAutomationController.send('Success'); },"
      "        reason => {"
      "      domAutomationController.send(reason.name + ': ' + reason.message);"
      "  });",
      &rejection));
  EXPECT_THAT(rejection,
              testing::MatchesRegex("SecurityError: .*blocklisted UUID.*"));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, NavigateWithChooserCrossOrigin) {
  content::TestNavigationObserver observer(
      web_contents_, 1 /* number_of_navigations */,
      content::MessageLoopRunner::QuitMode::DEFERRED);

  EXPECT_TRUE(content::ExecuteScript(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{name: 'Hello'}]});"
      "document.location.href = \"https://google.com\";"));

  observer.Wait();
  EXPECT_FALSE(chrome::IsDeviceChooserShowingForTesting(browser()));
  EXPECT_EQ(GURL("https://google.com"), web_contents_->GetLastCommittedURL());
}

// The new Web Bluetooth permissions backend is currently implemented behind a
// feature flag.
// TODO(https://crbug.com/589228): Delete this class and convert all the tests
// using it to use WebBluetoothTest instead.
class WebBluetoothTestWithNewPermissionsBackendEnabled
    : public WebBluetoothTest {
 public:
  WebBluetoothTestWithNewPermissionsBackendEnabled() {
    feature_list_.InitAndEnableFeature(
        features::kWebBluetoothNewPermissionsBackend);
  }

  // Move-only class
  WebBluetoothTestWithNewPermissionsBackendEnabled(
      const WebBluetoothTestWithNewPermissionsBackendEnabled&) = delete;
  WebBluetoothTestWithNewPermissionsBackendEnabled& operator=(
      const WebBluetoothTestWithNewPermissionsBackendEnabled&) = delete;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       PRE_WebBluetoothPersistentIds) {
  auto* delegate = UseAndGetTestWebContentsDelegate();
  AddFakeDevice(kDeviceAddress);
  delegate->SetDeviceToSelect(kDeviceAddress);

  // Grant permission for the device with address |kDeviceAddress| and store its
  // WebBluetoothDeviceId in localStorage to retrieve it after the browser
  // restarts.
  auto request_device_result = content::EvalJs(web_contents_, R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device'}]});
            localStorage.setItem('requestDeviceId', device.id);
            return device.id;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())");
  const std::string& granted_id = request_device_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(granted_id));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       WebBluetoothPersistentIds) {
  auto* delegate = UseAndGetTestWebContentsDelegate();
  AddFakeDevice(kDeviceAddress);
  delegate->SetDeviceToSelect(kDeviceAddress);

  // At the moment, there is not a way for Web Bluetooth to return a list of the
  // previously granted Bluetooth devices, so use requestDevice here.
  // TODO(https://crbug.com/577953): Once there is an API that can return the
  // permitted Web Bluetooth devices, use that API instead.
  auto request_device_result = content::EvalJs(web_contents_, R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device'}]});
            return device.id;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())");
  const std::string& granted_id = request_device_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(granted_id));

  auto local_storage_get_item_result = content::EvalJs(web_contents_, R"(
        (async() => {
          return localStorage.getItem('requestDeviceId');
        })())");
  const std::string& prev_granted_id =
      local_storage_get_item_result.ExtractString();
  EXPECT_EQ(granted_id, prev_granted_id);
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       PRE_WebBluetoothScanningIdsNotPersistent) {
  auto* delegate = UseAndGetTestWebContentsDelegate();

  // Grant permission to scan for Bluetooth devices and store the detected
  // device's WebBluetoothDeviceId in localStorage to retrieve it after the
  // browser restarts.
  ASSERT_TRUE(content::ExecJs(web_contents_, R"(
      var requestLEScanPromise = navigator.bluetooth.requestLEScan({
        acceptAllAdvertisements: true});
  )"));
  delegate->RunPromptEventHandler(
      content::BluetoothScanningPrompt::Event::kAllow);
  ASSERT_TRUE(content::ExecJs(web_contents_, "requestLEScanPromise"));

  ASSERT_TRUE(content::ExecJs(web_contents_, R"(
        var advertisementreceivedPromise = new Promise(resolve => {
          navigator.bluetooth.addEventListener('advertisementreceived',
              event => {
                localStorage.setItem('requestLEScanId', event.device.id);
                resolve(event.device.id);
              });
        });
      )"));

  SimulateDeviceAdvertisement(kDeviceAddress);

  auto advertisementreceived_promise_result =
      content::EvalJs(web_contents_, "advertisementreceivedPromise ");
  const std::string& scan_id =
      advertisementreceived_promise_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(scan_id));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       WebBluetoothScanningIdsNotPersistent) {
  auto* delegate = UseAndGetTestWebContentsDelegate();

  // Grant permission to scan for Bluetooth devices again, and compare the ID
  // assigned to the scanned device against the one that was stored previously.
  ASSERT_TRUE(content::ExecJs(web_contents_, R"(
      var requestLEScanPromise = navigator.bluetooth.requestLEScan({
        acceptAllAdvertisements: true});
  )"));
  delegate->RunPromptEventHandler(
      content::BluetoothScanningPrompt::Event::kAllow);
  ASSERT_TRUE(content::ExecJs(web_contents_, "requestLEScanPromise"));

  ASSERT_TRUE(content::ExecJs(web_contents_, R"(
        var advertisementreceivedPromise = new Promise(resolve => {
          navigator.bluetooth.addEventListener('advertisementreceived',
              event => {
                resolve(event.device.id);
              });
        });
      )"));

  SimulateDeviceAdvertisement(kDeviceAddress);

  auto advertisementreceived_promise_result =
      content::EvalJs(web_contents_, "advertisementreceivedPromise ");
  const std::string& scan_id =
      advertisementreceived_promise_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(scan_id));

  auto local_storage_get_item_result =
      content::EvalJs(web_contents_, "localStorage.getItem('requestLEScanId')");
  const std::string& prev_scan_id =
      local_storage_get_item_result.ExtractString();
  EXPECT_NE(scan_id, prev_scan_id);
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       PRE_WebBluetoothIdsUsedInWebBluetoothScanning) {
  auto* delegate = UseAndGetTestWebContentsDelegate();
  AddFakeDevice(kDeviceAddress);
  delegate->SetDeviceToSelect(kDeviceAddress);

  // Grant permission for the device with address |kDeviceAddress| and store its
  // WebBluetoothDeviceId in localStorage to retrieve it after the browser
  // restarts.
  auto request_device_result = content::EvalJs(web_contents_, R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device'}]});
            localStorage.setItem('requestDeviceId', device.id);
            return device.id;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())");
  const std::string& granted_id = request_device_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(granted_id));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       WebBluetoothIdsUsedInWebBluetoothScanning) {
  auto* delegate = UseAndGetTestWebContentsDelegate();

  // Grant permission to scan for Bluetooth devices again, and compare the ID
  // assigned to the scanned device against the one that was stored previously.
  ASSERT_TRUE(content::ExecJs(web_contents_, R"(
      var requestLEScanPromise = navigator.bluetooth.requestLEScan({
        acceptAllAdvertisements: true});
  )"));
  delegate->RunPromptEventHandler(
      content::BluetoothScanningPrompt::Event::kAllow);
  ASSERT_TRUE(content::ExecJs(web_contents_, "requestLEScanPromise"));

  ASSERT_TRUE(content::ExecJs(web_contents_, R"(
        var advertisementreceivedPromise = new Promise(resolve => {
          navigator.bluetooth.addEventListener('advertisementreceived',
              event => {
                resolve(event.device.id);
              });
        });
      )"));

  SimulateDeviceAdvertisement(kDeviceAddress);

  auto advertisementreceived_promise_result =
      content::EvalJs(web_contents_, "advertisementreceivedPromise ");
  const std::string& scan_id =
      advertisementreceived_promise_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(scan_id));

  auto local_storage_get_item_result =
      content::EvalJs(web_contents_, "localStorage.getItem('requestDeviceId')");
  const std::string& granted_id = local_storage_get_item_result.ExtractString();
  EXPECT_EQ(scan_id, granted_id);
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       PRE_WebBluetoothPersistentServices) {
  auto* delegate = UseAndGetTestWebContentsDelegate();
  AddFakeDevice(kDeviceAddress);
  delegate->SetDeviceToSelect(kDeviceAddress);

  // Grant permission for the device with address |kDeviceAddress| and store its
  // WebBluetoothDeviceId in localStorage to retrieve it after the browser
  // restarts.
  EXPECT_EQ(kHeartRateUUIDString,
            content::EvalJs(web_contents_, R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device', services: ['heart_rate']}]});
            let gatt = await device.gatt.connect();
            let service = await gatt.getPrimaryService('heart_rate');
            return service.uuid;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())"));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       WebBluetoothPersistentServices) {
  auto* delegate = UseAndGetTestWebContentsDelegate();
  AddFakeDevice(kDeviceAddress);
  delegate->SetDeviceToSelect(kDeviceAddress);

  // At the moment, there is not a way for Web Bluetooth to return a list of the
  // previously granted Bluetooth devices, so use requestDevice here without
  // specifying a filter for services. The site should still be able to GATT
  // connect and get the primary 'heart_rate' GATT service.
  // TODO(https://crbug.com/577953): Once there is an API that can return the
  // permitted Web Bluetooth devices, use that API instead.
  EXPECT_EQ(kHeartRateUUIDString,
            content::EvalJs(web_contents_, R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device'}]});
            let gatt = await device.gatt.connect();
            let service = await gatt.getPrimaryService('heart_rate');
            return service.uuid;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())"));
}

}  // namespace
