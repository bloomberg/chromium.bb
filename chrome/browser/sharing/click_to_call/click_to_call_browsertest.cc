// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_browsertest.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

namespace {
const char kTelUrl[] = "tel:+9876543210";
const char kNonTelUrl[] = "https://google.com";

const char kTextWithPhoneNumber[] = "call 9876543210 now";
const char kTextWithoutPhoneNumber[] = "abcde";

const char kTestPageURL[] = "/sharing/tel.html";
}  // namespace

// Browser tests for the Click To Call feature.
class ClickToCallBrowserTestBase : public SharingBrowserTest {
 public:
  ClickToCallBrowserTestBase() {}

  ~ClickToCallBrowserTestBase() override {}

  std::string GetTestPageURL() const override {
    return std::string(kTestPageURL);
  }

  void SetUpDevices(int count) {
    SharingBrowserTest::SetUpDevices(
        count, sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  }

  void CheckLastSharingMessageSent(
      const std::string& expected_phone_number) const {
    chrome_browser_sharing::SharingMessage sharing_message =
        GetLastSharingMessageSent();
    ASSERT_TRUE(sharing_message.has_click_to_call_message());
    EXPECT_EQ(expected_phone_number,
              sharing_message.click_to_call_message().phone_number());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClickToCallBrowserTestBase);
};

class ClickToCallBrowserTest : public ClickToCallBrowserTestBase {
 public:
  ClickToCallBrowserTest() {
    feature_list_.InitWithFeatures({kSharingDeviceRegistration, kClickToCallUI,
                                    kClickToCallContextMenuForSelectedText},
                                   {});
  }
};

// TODO(himanshujaju): Add UI checks.
IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_TelLink_SingleDeviceAvailable) {
  Init();
  SetUpDevices(/*count=*/1);

  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(1u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithoutPhoneNumber));

  // Check click to call items in context menu
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);
  CheckLastReceiver(devices[0]->guid());
  CheckLastSharingMessageSent(GetUnescapedURLContent(GURL(kTelUrl)));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_NoDevicesAvailable) {
  Init();
  AwaitQuiescence();

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithoutPhoneNumber));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_DevicesAvailable_SyncTurnedOff) {
  Init();
  SetUpDevices(/*count=*/1);
  // Disable syncing preferences which is necessary for Sharing.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(false, {});
  AwaitQuiescence();

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithoutPhoneNumber));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_TelLink_MultipleDevicesAvailable) {
  Init();
  SetUpDevices(/*count=*/2);

  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithoutPhoneNumber));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  ui::MenuModel* sub_menu_model = nullptr;
  int device_id = -1;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(kSubMenuFirstDeviceCommandId,
                                             &sub_menu_model, &device_id));
  EXPECT_EQ(2, sub_menu_model->GetItemCount());
  EXPECT_EQ(0, device_id);

  for (auto& device : devices) {
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + device_id,
              sub_menu_model->GetCommandIdAt(device_id));
    sub_menu_model->ActivatedAt(device_id);

    CheckLastReceiver(device->guid());
    CheckLastSharingMessageSent(GetUnescapedURLContent(GURL(kTelUrl)));
    device_id++;
  }
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_HighlightedText_MultipleDevicesAvailable) {
  Init();
  SetUpDevices(/*count=*/2);

  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kNonTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithPhoneNumber));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  ui::MenuModel* sub_menu_model = nullptr;
  int device_id = -1;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(kSubMenuFirstDeviceCommandId,
                                             &sub_menu_model, &device_id));
  EXPECT_EQ(2, sub_menu_model->GetItemCount());
  EXPECT_EQ(0, device_id);

  for (auto& device : devices) {
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + device_id,
              sub_menu_model->GetCommandIdAt(device_id));
    sub_menu_model->ActivatedAt(device_id);

    CheckLastReceiver(device->guid());
    base::Optional<std::string> expected_number =
        ExtractPhoneNumberForClickToCall(GetProfile(0), kTextWithPhoneNumber);
    ASSERT_TRUE(expected_number.has_value());
    CheckLastSharingMessageSent(expected_number.value());
    device_id++;
  }
}

class ClickToCallBrowserTestWithContextMenuDisabled
    : public ClickToCallBrowserTestBase {
 public:
  ClickToCallBrowserTestWithContextMenuDisabled() {
    feature_list_.InitWithFeatures({kSharingDeviceRegistration, kClickToCallUI},
                                   {kClickToCallContextMenuForSelectedText});
  }
};

IN_PROC_BROWSER_TEST_F(
    ClickToCallBrowserTestWithContextMenuDisabled,
    ContextMenu_HighlightedText_DevicesAvailable_FeatureFlagOff) {
  Init();
  SetUpDevices(/*count=*/2);

  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kNonTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithPhoneNumber));

  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_UKM) {
  Init();
  SetUpDevices(/*count=*/1);

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::RunLoop run_loop;
  ukm_recorder.SetOnAddEntryCallback(
      ukm::builders::Sharing_ClickToCall::kEntryName, run_loop.QuitClosure());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kNonTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithPhoneNumber));

  // Check click to call items in context menu
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  // Send number to device
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);

  // Expect UKM metrics to be logged
  run_loop.Run();
  std::vector<const ukm::mojom::UkmEntry*> ukm_entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::Sharing_ClickToCall::kEntryName);
  ASSERT_EQ(1u, ukm_entries.size());

  const int64_t* entry_point = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kEntryPointName);
  const int64_t* has_apps = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kHasAppsName);
  const int64_t* has_devices = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kHasDevicesName);
  const int64_t* selection = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kSelectionName);

  ASSERT_TRUE(entry_point);
  ASSERT_TRUE(has_apps);
  ASSERT_TRUE(has_devices);
  ASSERT_TRUE(selection);

  EXPECT_EQ(
      static_cast<int64_t>(SharingClickToCallEntryPoint::kRightClickSelection),
      *entry_point);
  EXPECT_EQ(true, *has_devices);
  EXPECT_EQ(static_cast<int64_t>(SharingClickToCallSelection::kDevice),
            *selection);
  // TODO(knollr): mock apps and verify |has_apps| here too.
}

class ClickToCallBrowserTestWithContextMenuFeatureDefault
    : public ClickToCallBrowserTestBase {
 public:
  ClickToCallBrowserTestWithContextMenuFeatureDefault() {
    feature_list_.InitWithFeatures({kSharingDeviceRegistration, kClickToCallUI},
                                   {});
  }
};

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, CloseTabWithBubble) {
  Init();
  SetUpDevices(/*count=*/1);

  base::RunLoop run_loop;
  ClickToCallUiController::GetOrCreateFromWebContents(web_contents())
      ->set_on_dialog_shown_closure_for_testing(run_loop.QuitClosure());

  // Click on the tel link to trigger the bubble view.
  web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.querySelector('a').click();"),
      base::NullCallback());
  // Wait until the bubble is visible.
  run_loop.Run();

  // Close the tab while the bubble is opened.
  // Regression test for http://crbug.com/1000934.
  sessions_helper::CloseTab(/*browser_index=*/0, /*tab_index=*/0);
}
