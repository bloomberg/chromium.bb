// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/hid/hid_collection_info.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_usage_and_page.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"

using base::ThreadTaskRunnerHandle;
using device::HidCollectionInfo;
using device::HidPlatformDeviceId;
using device::HidUsageAndPage;

#if defined(OS_MACOSX)
const uint64_t kTestDeviceIds[] = {1, 2, 3, 4, 5};
#else
const char* const kTestDeviceIds[] = {"A", "B", "C", "D", "E"};
#endif

// These report descriptors define two devices with 8-byte input, output and
// feature reports. The first implements usage page 0xFF00 and has a single
// report without and ID. The second implements usage page 0xFF01 and has a
// single report with ID 1.
const uint8_t kReportDescriptor[] = {0x06, 0x00, 0xFF, 0x08, 0xA1, 0x01, 0x15,
                                     0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95,
                                     0x08, 0x08, 0x81, 0x02, 0x08, 0x91, 0x02,
                                     0x08, 0xB1, 0x02, 0xC0};
const uint8_t kReportDescriptorWithIDs[] = {
    0x06, 0x01, 0xFF, 0x08, 0xA1, 0x01, 0x15, 0x00, 0x26,
    0xFF, 0x00, 0x85, 0x01, 0x75, 0x08, 0x95, 0x08, 0x08,
    0x81, 0x02, 0x08, 0x91, 0x02, 0x08, 0xB1, 0x02, 0xC0};

namespace extensions {

class FakeHidConnectionImpl : public device::mojom::HidConnection {
 public:
  explicit FakeHidConnectionImpl(scoped_refptr<device::HidDeviceInfo> device)
      : device_(device) {}

  ~FakeHidConnectionImpl() override = default;

  // device::mojom::HidConnection implemenation:
  void Read(device::mojom::HidConnection::ReadCallback callback) override {
    const char kResult[] = "This is a HID input report.";
    uint8_t report_id = device_->has_report_id() ? 1 : 0;

    std::vector<uint8_t> buffer(kResult, kResult + sizeof(kResult) - 1);

    std::move(callback).Run(true, report_id, buffer);
  }

  void Write(uint8_t report_id,
             const std::vector<uint8_t>& buffer,
             device::mojom::HidConnection::WriteCallback callback) override {
    const char kExpected[] = "o-report";  // 8 bytes
    if (buffer.size() != sizeof(kExpected) - 1) {
      std::move(callback).Run(false);
      return;
    }

    int expected_report_id = device_->has_report_id() ? 1 : 0;
    if (report_id != expected_report_id) {
      std::move(callback).Run(false);
      return;
    }

    if (memcmp(buffer.data(), kExpected, sizeof(kExpected) - 1) != 0) {
      std::move(callback).Run(false);
      return;
    }

    std::move(callback).Run(true);
  }

  void GetFeatureReport(uint8_t report_id,
                        device::mojom::HidConnection::GetFeatureReportCallback
                            callback) override {
    uint8_t expected_report_id = device_->has_report_id() ? 1 : 0;
    if (report_id != expected_report_id) {
      std::move(callback).Run(false, base::nullopt);
      return;
    }

    const char kResult[] = "This is a HID feature report.";
    std::vector<uint8_t> buffer;
    if (device_->has_report_id())
      buffer.push_back(report_id);
    buffer.insert(buffer.end(), kResult, kResult + sizeof(kResult) - 1);

    std::move(callback).Run(true, buffer);
  }

  void SendFeatureReport(uint8_t report_id,
                         const std::vector<uint8_t>& buffer,
                         device::mojom::HidConnection::SendFeatureReportCallback
                             callback) override {
    const char kExpected[] = "The app is setting this HID feature report.";
    if (buffer.size() != sizeof(kExpected) - 1) {
      std::move(callback).Run(false);
      return;
    }

    int expected_report_id = device_->has_report_id() ? 1 : 0;
    if (report_id != expected_report_id) {
      std::move(callback).Run(false);
      return;
    }

    if (memcmp(buffer.data(), kExpected, sizeof(kExpected) - 1) != 0) {
      std::move(callback).Run(false);
      return;
    }

    std::move(callback).Run(true);
  }

 private:
  scoped_refptr<device::HidDeviceInfo> device_;
};

class FakeHidManager : public device::mojom::HidManager {
 public:
  FakeHidManager() {}
  ~FakeHidManager() override = default;

  void Bind(const std::string& interface_name,
            mojo::ScopedMessagePipeHandle handle,
            const service_manager::BindSourceInfo& source_info) {
    bindings_.AddBinding(this,
                         device::mojom::HidManagerRequest(std::move(handle)));
  }

  // device::mojom::HidManager implementation:
  void GetDevicesAndSetClient(
      device::mojom::HidManagerClientAssociatedPtrInfo client,
      device::mojom::HidManager::GetDevicesCallback callback) override {
    std::vector<device::mojom::HidDeviceInfoPtr> device_list;
    for (auto& map_entry : devices_)
      device_list.push_back(map_entry.second->device()->Clone());

    std::move(callback).Run(std::move(device_list));

    device::mojom::HidManagerClientAssociatedPtr client_ptr;
    client_ptr.Bind(std::move(client));
    clients_.AddPtr(std::move(client_ptr));
  }

  void GetDevices(
      device::mojom::HidManager::GetDevicesCallback callback) override {
    // Clients of HidManager in extensions only use GetDevicesAndSetClient().
    NOTREACHED();
  }

  void Connect(const std::string& device_guid,
               device::mojom::HidManager::ConnectCallback callback) override {
    scoped_refptr<device::HidDeviceInfo> device = devices_[device_guid];

    // Strong binds a instance of FakeHidConnctionImpl.
    device::mojom::HidConnectionPtr client;
    mojo::MakeStrongBinding(base::MakeUnique<FakeHidConnectionImpl>(device),
                            mojo::MakeRequest(&client));
    std::move(callback).Run(std::move(client));
  }

  void AddDevice(scoped_refptr<device::HidDeviceInfo> device) {
    devices_[device->device_guid()] = device;

    device::mojom::HidDeviceInfo* device_info = device->device().get();
    clients_.ForAllPtrs([device_info](device::mojom::HidManagerClient* client) {
      client->DeviceAdded(device_info->Clone());
    });
  }

  void RemoveDevice(const HidPlatformDeviceId& platform_device_id) {
    std::string guid;
    scoped_refptr<device::HidDeviceInfo> device;
    for (const auto& map_entry : devices_) {
      if (map_entry.second->platform_device_id() == platform_device_id) {
        guid = map_entry.first;
        device = map_entry.second;
        break;
      }
    }

    if (!guid.empty()) {
      device::mojom::HidDeviceInfo* device_info = device->device().get();
      clients_.ForAllPtrs(
          [device_info](device::mojom::HidManagerClient* client) {
            client->DeviceRemoved(device_info->Clone());
          });
      devices_.erase(guid);
    }
  }

 private:
  std::map<std::string, scoped_refptr<device::HidDeviceInfo>> devices_;
  mojo::AssociatedInterfacePtrSet<device::mojom::HidManagerClient> clients_;
  mojo::BindingSet<device::mojom::HidManager> bindings_;
};

class TestDevicePermissionsPrompt
    : public DevicePermissionsPrompt,
      public DevicePermissionsPrompt::Prompt::Observer {
 public:
  explicit TestDevicePermissionsPrompt(content::WebContents* web_contents)
      : DevicePermissionsPrompt(web_contents) {}

  ~TestDevicePermissionsPrompt() override { prompt()->SetObserver(nullptr); }

  void ShowDialog() override { prompt()->SetObserver(this); }

  void OnDeviceAdded(size_t index, const base::string16& device_name) override {
    OnDevicesChanged();
  }

  void OnDeviceRemoved(size_t index,
                       const base::string16& device_name) override {
    OnDevicesChanged();
  }

 private:
  void OnDevicesChanged() {
    if (prompt()->multiple()) {
      for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
        prompt()->GrantDevicePermission(i);
      }
      prompt()->Dismissed();
    } else {
      for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
        // Always choose the device whose serial number is "A".
        if (prompt()->GetDeviceSerialNumber(i) == base::UTF8ToUTF16("A")) {
          prompt()->GrantDevicePermission(i);
          prompt()->Dismissed();
          return;
        }
      }
    }
  }
};

class TestExtensionsAPIClient : public ShellExtensionsAPIClient {
 public:
  TestExtensionsAPIClient() : ShellExtensionsAPIClient() {}

  std::unique_ptr<DevicePermissionsPrompt> CreateDevicePermissionsPrompt(
      content::WebContents* web_contents) const override {
    return std::make_unique<TestDevicePermissionsPrompt>(web_contents);
  }
};

class HidApiTest : public ShellApiTest {
 public:
  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    fake_hid_manager_ = base::MakeUnique<FakeHidManager>();
    // Because Device Service also runs in this process(browser process), here
    // we can directly set our binder to intercept interface requests against
    // it.
    service_manager::ServiceContext::SetGlobalBinderForTesting(
        device::mojom::kServiceName, device::mojom::HidManager::Name_,
        base::Bind(&FakeHidManager::Bind,
                   base::Unretained(fake_hid_manager_.get())));

    AddDevice(kTestDeviceIds[0], 0x18D1, 0x58F0, false, "A");
    AddDevice(kTestDeviceIds[1], 0x18D1, 0x58F0, true, "B");
    AddDevice(kTestDeviceIds[2], 0x18D1, 0x58F1, false, "C");
  }

  void AddDevice(const HidPlatformDeviceId& platform_device_id,
                 int vendor_id,
                 int product_id,
                 bool report_id,
                 std::string serial_number) {
    std::vector<uint8_t> report_descriptor;
    if (report_id) {
      report_descriptor.insert(
          report_descriptor.begin(), kReportDescriptorWithIDs,
          kReportDescriptorWithIDs + sizeof(kReportDescriptorWithIDs));
    } else {
      report_descriptor.insert(report_descriptor.begin(), kReportDescriptor,
                               kReportDescriptor + sizeof(kReportDescriptor));
    }
    fake_hid_manager_->AddDevice(new device::HidDeviceInfo(
        platform_device_id, vendor_id, product_id, "Test Device", serial_number,
        device::mojom::HidBusType::kHIDBusTypeUSB, report_descriptor));
  }

  FakeHidManager* GetFakeHidManager() { return fake_hid_manager_.get(); }

 protected:
  std::unique_ptr<FakeHidManager> fake_hid_manager_;
};

IN_PROC_BROWSER_TEST_F(HidApiTest, HidApp) {
  ASSERT_TRUE(RunAppTest("api_test/hid/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(HidApiTest, OnDeviceAdded) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/hid/add_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Add a blocked device first so that the test will fail if a notification is
  // received.
  AddDevice(kTestDeviceIds[3], 0x18D1, 0x58F1, false, "A");
  AddDevice(kTestDeviceIds[4], 0x18D1, 0x58F0, false, "A");
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", result_listener.message());
}

IN_PROC_BROWSER_TEST_F(HidApiTest, OnDeviceRemoved) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/hid/remove_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Device C was not returned by chrome.hid.getDevices, the app will not get
  // a notification.
  GetFakeHidManager()->RemoveDevice(kTestDeviceIds[2]);
  // Device A was returned, the app will get a notification.
  GetFakeHidManager()->RemoveDevice(kTestDeviceIds[0]);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", result_listener.message());
}

IN_PROC_BROWSER_TEST_F(HidApiTest, GetUserSelectedDevices) {
  ExtensionTestMessageListener open_listener("opened_device", false);

  TestExtensionsAPIClient test_api_client;
  ASSERT_TRUE(LoadApp("api_test/hid/get_user_selected_devices"));
  ASSERT_TRUE(open_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener remove_listener("removed", false);
  GetFakeHidManager()->RemoveDevice(kTestDeviceIds[0]);
  ASSERT_TRUE(remove_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener add_listener("added", false);
  AddDevice(kTestDeviceIds[0], 0x18D1, 0x58F0, true, "A");
  ASSERT_TRUE(add_listener.WaitUntilSatisfied());
}

}  // namespace extensions
