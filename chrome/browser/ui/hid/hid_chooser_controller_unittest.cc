// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/test/web_contents_tester.h"
#include "services/device/public/cpp/hid/fake_hid_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

const uint16_t kYubicoVendorId = 0x1050;
const uint16_t kYubicoGnubbyProductId = 0x0200;

class FakeHidChooserView : public ChooserController::View {
 public:
  FakeHidChooserView() {}

  void set_options_initialized_quit_closure(base::OnceClosure quit_closure) {
    options_initialized_quit_closure_ = std::move(quit_closure);
  }

  // ChooserController::View:
  void OnOptionAdded(size_t index) override {}
  void OnOptionRemoved(size_t index) override {}
  void OnOptionsInitialized() override {
    if (options_initialized_quit_closure_)
      std::move(options_initialized_quit_closure_).Run();
  }
  void OnOptionUpdated(size_t index) override {}
  void OnAdapterEnabledChanged(bool enabled) override {}
  void OnRefreshStateChanged(bool enabled) override {}

 private:
  base::OnceClosure options_initialized_quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(FakeHidChooserView);
};

}  // namespace

class HidChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  HidChooserControllerTest() {}

  void SetUp() override {
    OverrideChooserStrings();
    ChromeRenderViewHostTestHarness::SetUp();

    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

    // Set fake HID manager for HidChooserContext.
    device::mojom::HidManagerPtr hid_manager_ptr;
    hid_manager_.Bind(mojo::MakeRequest(&hid_manager_ptr));
    HidChooserContextFactory::GetForProfile(profile())->SetHidManagerForTesting(
        std::move(hid_manager_ptr));

    hid_chooser_controller_ = std::make_unique<HidChooserController>(
        main_rfh(), content::HidChooser::Callback());
    fake_hid_chooser_view_ = std::make_unique<FakeHidChooserView>();
    hid_chooser_controller_->set_view(fake_hid_chooser_view_.get());
  }

  void OverrideChooserStrings() {
    // Release builds may strip out unused string resources when
    // enable_resource_whitelist_generation is enabled. Manually override the
    // strings needed by the chooser to ensure they are available for tests.
    // TODO(mattreynolds): Remove these overrides once the strings are
    // referenced from the Chrome binary.
    auto& shared_resource_bundle = ui::ResourceBundle::GetSharedInstance();
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_HID_CHOOSER_PROMPT_ORIGIN,
        base::ASCIIToUTF16("$1 wants to connect to a HID device"));
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_HID_CHOOSER_PROMPT_EXTENSION_NAME,
        base::ASCIIToUTF16("\"$1\" wants to connect to a HID device"));
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_HID_CHOOSER_ITEM_WITHOUT_NAME,
        base::ASCIIToUTF16("Unknown Device (Vendor: $1, Product: $2)"));
    shared_resource_bundle.OverrideLocaleStringResource(
        IDS_HID_CHOOSER_ITEM_WITH_NAME,
        base::ASCIIToUTF16("$1 (Vendor: $2, Product: $3)"));
  }

  device::mojom::HidDeviceInfoPtr CreateAndAddFakeHidDevice(
      uint32_t vendor_id,
      uint16_t product_id,
      const std::string& product_string,
      const std::string& serial_number) {
    return hid_manager_.CreateAndAddDevice(
        vendor_id, product_id, product_string, serial_number,
        device::mojom::HidBusType::kHIDBusTypeUSB);
  }

 protected:
  device::FakeHidManager hid_manager_;
  std::unique_ptr<HidChooserController> hid_chooser_controller_;
  std::unique_ptr<FakeHidChooserView> fake_hid_chooser_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HidChooserControllerTest);
};

TEST_F(HidChooserControllerTest, EmptyChooser) {
  base::RunLoop run_loop;
  fake_hid_chooser_view_->set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(0u, hid_chooser_controller_->NumOptions());
}

TEST_F(HidChooserControllerTest, AddBlockedFidoDevice) {
  // FIDO U2F devices (and other devices on the USB blocklist) should be
  // excluded from the device chooser.
  base::RunLoop run_loop;
  fake_hid_chooser_view_->set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(kYubicoVendorId, kYubicoGnubbyProductId, "gnubby",
                            "001");
  run_loop.Run();
  EXPECT_EQ(0u, hid_chooser_controller_->NumOptions());
}

TEST_F(HidChooserControllerTest, AddNamedDevice) {
  base::RunLoop run_loop;
  fake_hid_chooser_view_->set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "a", "001");
  run_loop.Run();
  EXPECT_EQ(1u, hid_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a (Vendor: 0x0001, Product: 0x0001)"),
            hid_chooser_controller_->GetOption(0));
}

TEST_F(HidChooserControllerTest, AddUnnamedDevice) {
  base::RunLoop run_loop;
  fake_hid_chooser_view_->set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "", "001");
  run_loop.Run();
  EXPECT_EQ(1u, hid_chooser_controller_->NumOptions());
  EXPECT_EQ(
      base::ASCIIToUTF16("Unknown Device (Vendor: 0x0001, Product: 0x0001)"),
      hid_chooser_controller_->GetOption(0));
}
