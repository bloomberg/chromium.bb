// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_service_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/web_contents_tester.h"
#include "device/base/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::AtMost;

using blink::mojom::WebUsbServicePtr;
using device::mojom::UsbDeviceInfo;
using device::mojom::UsbDeviceInfoPtr;
using device::mojom::UsbDeviceManagerClient;
using device::mojom::UsbDeviceManagerClientAssociatedPtrInfo;
using device::MockUsbDevice;
using device::UsbDevice;

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

ACTION_P2(ExpectGuidAndThen, expected_guid, callback) {
  ASSERT_TRUE(arg0);
  EXPECT_EQ(expected_guid, arg0->guid);
  if (!callback.is_null())
    callback.Run();
};

class WebUsbServiceImplTest : public ChromeRenderViewHostTestHarness {
 public:
  WebUsbServiceImplTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  }

 protected:
  WebUsbServicePtr ConnectToService() {
    WebUsbServicePtr servicePtr;
    web_usb_service_.reset(new WebUsbServiceImpl(main_rfh(), nullptr));
    web_usb_service_->BindRequest(mojo::MakeRequest(&servicePtr));
    return servicePtr;
  }

  void GrantDevicePermission(const GURL& requesting_origin,
                             const GURL& embedding_origin,
                             const device::mojom::UsbDeviceInfo& device_info) {
    UsbChooserContextFactory::GetForProfile(profile())->GrantDevicePermission(
        requesting_origin, embedding_origin, device_info);
  }

  device::MockDeviceClient device_client_;

 private:
  std::unique_ptr<WebUsbServiceImpl> web_usb_service_;
  DISALLOW_COPY_AND_ASSIGN(WebUsbServiceImplTest);
};

class MockDeviceManagerClient : public UsbDeviceManagerClient {
 public:
  MockDeviceManagerClient() : binding_(this) {}
  ~MockDeviceManagerClient() override = default;

  UsbDeviceManagerClientAssociatedPtrInfo CreateInterfacePtrAndBind() {
    UsbDeviceManagerClientAssociatedPtrInfo client;
    binding_.Bind(mojo::MakeRequest(&client));
    return client;
  }

  MOCK_METHOD1(DoOnDeviceAdded, void(UsbDeviceInfo*));
  void OnDeviceAdded(UsbDeviceInfoPtr device_info) override {
    DoOnDeviceAdded(device_info.get());
  }

  MOCK_METHOD1(DoOnDeviceRemoved, void(UsbDeviceInfo*));
  void OnDeviceRemoved(UsbDeviceInfoPtr device_info) override {
    DoOnDeviceRemoved(device_info.get());
  }

 private:
  mojo::AssociatedBinding<UsbDeviceManagerClient> binding_;
};

void ExpectDevicesAndThen(const std::set<std::string>& expected_guids,
                          base::OnceClosure continuation,
                          std::vector<UsbDeviceInfoPtr> results) {
  EXPECT_EQ(expected_guids.size(), results.size());
  std::set<std::string> actual_guids;
  for (size_t i = 0; i < results.size(); ++i)
    actual_guids.insert(results[i]->guid);
  EXPECT_EQ(expected_guids, actual_guids);
  std::move(continuation).Run();
}

}  // namespace

TEST_F(WebUsbServiceImplTest, NoPermissionDevice) {
  GURL origin(kDefaultTestUrl);

  scoped_refptr<UsbDevice> device0 =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  scoped_refptr<UsbDevice> device1 =
      new MockUsbDevice(0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  scoped_refptr<UsbDevice> no_permission_device1 =
      new MockUsbDevice(0xffff, 0x567b, "ACME", "Frobinator II", "MNOPQR");
  scoped_refptr<UsbDevice> no_permission_device2 =
      new MockUsbDevice(0xffff, 0x567c, "ACME", "Frobinator Xtreme", "STUVWX");

  auto device_info_0 = device::mojom::UsbDeviceInfo::From(*device0);
  auto device_info_1 = device::mojom::UsbDeviceInfo::From(*device1);

  device_client_.usb_service()->AddDevice(device0);
  GrantDevicePermission(origin, origin, *device_info_0);
  device_client_.usb_service()->AddDevice(no_permission_device1);

  WebUsbServicePtr web_usb_service = ConnectToService();
  MockDeviceManagerClient mock_client;
  web_usb_service->SetClient(mock_client.CreateInterfacePtrAndBind());

  {
    // Call GetDevices once to make sure the WebUsbService is up and running
    // and the client is set or else we could block forever waiting for calls.
    // The site has no permission to access |no_permission_device1|, so result
    // of GetDevices() should only contain the |guid| of |device0|.
    std::set<std::string> guids;
    guids.insert(device0->guid());
    base::RunLoop loop;
    web_usb_service->GetDevices(
        base::BindOnce(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
    loop.Run();
  }

  device_client_.usb_service()->AddDevice(device1);
  GrantDevicePermission(origin, origin, *device_info_1);
  device_client_.usb_service()->AddDevice(no_permission_device2);
  device_client_.usb_service()->RemoveDevice(device0);
  device_client_.usb_service()->RemoveDevice(device1);
  device_client_.usb_service()->RemoveDevice(no_permission_device1);
  device_client_.usb_service()->RemoveDevice(no_permission_device2);
  {
    base::RunLoop loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, loop.QuitClosure());
    testing::InSequence s;

    EXPECT_CALL(mock_client, DoOnDeviceRemoved(_))
        .WillOnce(ExpectGuidAndThen(device0->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device1->guid(), barrier));
    loop.Run();
  }

  device_client_.usb_service()->AddDevice(device0);
  device_client_.usb_service()->AddDevice(device1);
  device_client_.usb_service()->AddDevice(no_permission_device1);
  device_client_.usb_service()->AddDevice(no_permission_device2);
  {
    base::RunLoop loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, loop.QuitClosure());
    testing::InSequence s;

    EXPECT_CALL(mock_client, DoOnDeviceAdded(_))
        .WillOnce(ExpectGuidAndThen(device0->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device1->guid(), barrier));
    loop.Run();
  }
}
