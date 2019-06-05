// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "device/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace content {

namespace {

const char kBatteryServiceUUIDString[] = "0000180f-0000-1000-8000-00805f9b34fb";

class FakeBluetoothScanningPrompt : public BluetoothScanningPrompt {
 public:
  explicit FakeBluetoothScanningPrompt(const EventHandler& event_handler) {
    event_handler.Run(content::BluetoothScanningPrompt::Event::kAllow);
  }
  ~FakeBluetoothScanningPrompt() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothScanningPrompt);
};

class FakeBluetoothAdapter : public device::MockBluetoothAdapter {
 public:
  FakeBluetoothAdapter() = default;

  // device::BluetoothAdapter:
  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override {
    std::move(callback).Run(
        /*is_error=*/false,
        device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }

 protected:
  ~FakeBluetoothAdapter() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothAdapter);
};

class FakeWebContentsDelegate : public content::WebContentsDelegate {
 public:
  FakeWebContentsDelegate() = default;
  ~FakeWebContentsDelegate() override = default;

  std::unique_ptr<BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      RenderFrameHost* frame,
      const BluetoothScanningPrompt::EventHandler& event_handler) override {
    return std::make_unique<FakeBluetoothScanningPrompt>(event_handler);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeWebContentsDelegate);
};

}  // namespace

class WebBluetoothServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  WebBluetoothServiceImplTest() = default;
  ~WebBluetoothServiceImplTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    // Set up an adapter.
    scoped_refptr<FakeBluetoothAdapter> adapter(new FakeBluetoothAdapter());
    EXPECT_CALL(*adapter, IsPresent()).WillRepeatedly(Return(true));
    device::BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterForTesting(
        adapter);

    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    contents()->SetDelegate(&delegate_);

    // Simulate a frame connected to a bluetooth service.
    service_ =
        contents()->GetMainFrame()->CreateWebBluetoothServiceForTesting();

    blink::mojom::WebBluetoothScanClientAssociatedPtrInfo client_info;
    mojo::MakeRequest(&client_info);
    auto options = blink::mojom::WebBluetoothRequestLEScanOptions::New();
    base::Optional<std::vector<device::BluetoothUUID>> services;
    services.emplace();
    services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
    auto filter =
        blink::mojom::WebBluetoothLeScanFilter::New(services, "a", "b");
    options->filters.emplace();
    options->filters->push_back(filter.Clone());
    filters_.emplace();
    filters_->push_back(filter.Clone());
    service_->RequestScanningStart(
        std::move(client_info), std::move(options),
        base::BindLambdaForTesting(
            [&](blink::mojom::RequestScanningStartResultPtr p) {
              loop_.Quit();
            }));
    loop_.Run();
  }

  void TearDown() override {
    service_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  WebBluetoothServiceImpl* service_;
  base::Optional<WebBluetoothServiceImpl::ScanFilters> filters_;
  FakeWebContentsDelegate delegate_;
  base::RunLoop loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebBluetoothServiceImplTest);
};

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenTabHidden) {
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters_));

  contents()->SetVisibility(content::Visibility::HIDDEN);

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters_));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenTabOccluded) {
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters_));

  contents()->SetVisibility(content::Visibility::OCCLUDED);

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters_));
}

}  // namespace content
