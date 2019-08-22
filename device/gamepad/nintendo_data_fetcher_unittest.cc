// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/nintendo_data_fetcher.h"

#include <utility>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_pad_state_provider.h"
#include "device/gamepad/gamepad_provider.h"
#include "device/gamepad/gamepad_service.h"
#include "device/gamepad/nintendo_controller.h"
#include "services/device/device_service_test_base.h"
#include "services/device/hid/hid_device_info.h"
#include "services/device/hid/hid_manager_impl.h"
#include "services/device/hid/mock_hid_service.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/961039): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_AddAndRemoveSwitchPro DISABLED_AddAndRemoveSwitchPro
#define MAYBE_UnsupportedDeviceIsIgnored DISABLED_UnsupportedDeviceIsIgnored
#define MAYBE_ExceedGamepadLimit DISABLED_ExceedGamepadLimit
#else
#define MAYBE_AddAndRemoveSwitchPro AddAndRemoveSwitchPro
#define MAYBE_UnsupportedDeviceIsIgnored UnsupportedDeviceIsIgnored
#define MAYBE_ExceedGamepadLimit ExceedGamepadLimit
#endif

namespace device {

namespace {
constexpr uint16_t kVendorNintendo = 0x57e;
constexpr uint16_t kProductSwitchPro = 0x2009;
constexpr size_t kMaxInputReportSizeBytes = 63;
constexpr size_t kMaxOutputReportSizeBytes = 63;
constexpr size_t kMaxFeatureReportSizeBytes = 0;
}  // namespace

// Main test fixture
class NintendoDataFetcherTest : public DeviceServiceTestBase {
 public:
  NintendoDataFetcherTest() {}

  void SetUp() override {
    // Set up the fake HID service.
    auto mock_hid_service = std::make_unique<MockHidService>();
    mock_hid_service_ = mock_hid_service.get();
    mock_hid_service_->FirstEnumerationComplete();

    // Transfer the ownership of the |mock_hid_service| to HidManagerImpl.
    // It is safe to use |mock_hid_service_| in this test.
    HidManagerImpl::SetHidServiceForTesting(std::move(mock_hid_service));

    // Initialize the device service and pass a service connector to the gamepad
    // service.
    DeviceServiceTestBase::SetUp();
    GamepadService::GetInstance()->StartUp(connector()->Clone());

    // Create the data fetcher and polling thread.
    auto fetcher = std::make_unique<NintendoDataFetcher>();
    fetcher_ = fetcher.get();
    auto polling_thread = std::make_unique<base::Thread>("polling thread");
    polling_thread_ = polling_thread.get();
    provider_.reset(new GamepadProvider(nullptr, std::move(fetcher),
                                        std::move(polling_thread)));

    RunUntilIdle();
  }

  void TearDown() override {
    HidManagerImpl::SetHidServiceForTesting(nullptr);
    GamepadService::SetInstance(nullptr);
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
    polling_thread_->FlushForTesting();
  }

  // HidPlatformDeviceId is uint64_t on macOS, std::string elsewhere.
  HidPlatformDeviceId GetUniqueDeviceId() {
#if defined(OS_MACOSX)
    return next_device_id_++;
#else
    return base::StringPrintf("%" PRIu64, next_device_id_++);
#endif
  }

  scoped_refptr<HidDeviceInfo> CreateSwitchProUsb() {
    auto collection = mojom::HidCollectionInfo::New();
    collection->usage = mojom::HidUsageAndPage::New(
        mojom::kGenericDesktopJoystick, mojom::kPageGenericDesktop);
    return new HidDeviceInfo(GetUniqueDeviceId(), kVendorNintendo,
                             kProductSwitchPro, "Switch Pro Controller",
                             "test-serial", mojom::HidBusType::kHIDBusTypeUSB,
                             std::move(collection), kMaxInputReportSizeBytes,
                             kMaxOutputReportSizeBytes,
                             kMaxFeatureReportSizeBytes);
  }

  MockHidService* mock_hid_service_;
  std::unique_ptr<GamepadProvider> provider_;
  NintendoDataFetcher* fetcher_;
  base::Thread* polling_thread_;

  uint64_t next_device_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NintendoDataFetcherTest);
};

TEST_F(NintendoDataFetcherTest, MAYBE_UnsupportedDeviceIsIgnored) {
  // Simulate an unsupported, non-Nintendo HID device.
  auto collection = mojom::HidCollectionInfo::New();
  collection->usage = mojom::HidUsageAndPage::New(
      mojom::kGenericDesktopJoystick, mojom::kPageGenericDesktop);
  scoped_refptr<HidDeviceInfo> device_info(new HidDeviceInfo(
      GetUniqueDeviceId(), /*vendor_id=*/0x1234, /*product_id=*/0xabcd,
      "NonTendo", "test-serial", mojom::HidBusType::kHIDBusTypeUSB,
      std::move(collection), kMaxInputReportSizeBytes,
      kMaxOutputReportSizeBytes, kMaxFeatureReportSizeBytes));

  // Add the device to the mock HID service. The HID service should notify the
  // data fetcher.
  mock_hid_service_->AddDevice(device_info);
  RunUntilIdle();

  // The device should not have been added to the internal device map.
  EXPECT_TRUE(fetcher_->GetControllersForTesting().empty());

  // Remove the device.
  mock_hid_service_->RemoveDevice(device_info->platform_device_id());
  RunUntilIdle();
}

TEST_F(NintendoDataFetcherTest, MAYBE_AddAndRemoveSwitchPro) {
  // Simulate a Switch Pro over USB.
  scoped_refptr<HidDeviceInfo> device_info = CreateSwitchProUsb();

  // Add the device to the mock HID service. The HID service should notify the
  // data fetcher.
  mock_hid_service_->AddDevice(device_info);
  RunUntilIdle();

  // The fetcher should have added the device to its internal device map.
  EXPECT_EQ(1U, fetcher_->GetControllersForTesting().size());

  // Remove the device.
  mock_hid_service_->RemoveDevice(device_info->platform_device_id());

  RunUntilIdle();

  // Check that the device was removed.
  EXPECT_TRUE(fetcher_->GetControllersForTesting().empty());
}

TEST_F(NintendoDataFetcherTest, MAYBE_ExceedGamepadLimit) {
  // Simulate connecting Switch Pro gamepads until the limit is exceeded.
  const size_t kNumGamepads = Gamepads::kItemsLengthCap + 1;
  std::vector<HidPlatformDeviceId> device_ids;
  for (size_t i = 0; i < kNumGamepads; ++i) {
    auto device_info = CreateSwitchProUsb();
    device_ids.push_back(device_info->platform_device_id());
    mock_hid_service_->AddDevice(device_info);

    RunUntilIdle();

    // Simulate successful initialization. This must occur on the polling
    // thread.
    provider_->GetPollingThreadRunnerForTesting()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          auto& controller_map = fetcher_->GetControllersForTesting();
          for (auto& entry : controller_map)
            entry.second->FinishInitSequenceForTesting();
        }));

    RunUntilIdle();

    // Check that the gamepad was added.
    EXPECT_EQ(i + 1, fetcher_->GetControllersForTesting().size());
  }

  // Poll once. This allows the data fetcher to acquire PadState slots for
  // connected gamepads.
  provider_->PollOnceForTesting();
  RunUntilIdle();

  // Check that all PadState slots are assigned.
  EXPECT_EQ(nullptr,
            provider_->GetPadState(GAMEPAD_SOURCE_TEST, /*source_id=*/100));

  // Remove all gamepads.
  for (size_t i = 0; i < kNumGamepads; ++i) {
    mock_hid_service_->RemoveDevice(device_ids[i]);
    RunUntilIdle();

    // Check that the gamepad was removed.
    EXPECT_EQ(kNumGamepads - i - 1,
              fetcher_->GetControllersForTesting().size());
  }

  // Poll once. This allows the provider to detect that the gamepads have been
  // disconnected.
  provider_->PollOnceForTesting();
  RunUntilIdle();

  // After disconnecting the devices there should be unassigned PadState slots.
  EXPECT_NE(nullptr,
            provider_->GetPadState(GAMEPAD_SOURCE_TEST, /*source_id=*/100));
}

}  // namespace device
