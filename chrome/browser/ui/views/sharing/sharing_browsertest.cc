// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_browsertest.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_device_source_sync.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_utils.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/context_menu_data/media_type.h"

void FakeWebPushSender::SendMessage(const std::string& fcm_token,
                                    crypto::ECPrivateKey* vapid_key,
                                    WebPushMessage message,
                                    WebPushCallback callback) {
  fcm_token_ = fcm_token;
  message_ = std::move(message);
  std::move(callback).Run(SendWebPushMessageResult::kSuccessful, "message_id");
}

void FakeSharingMessageBridge::SendSharingMessage(
    std::unique_ptr<sync_pb::SharingMessageSpecifics> specifics,
    CommitFinishedCallback on_commit_callback) {
  specifics_ = std::move(*specifics);
  sync_pb::SharingMessageCommitError commit_erorr;
  commit_erorr.set_error_code(sync_pb::SharingMessageCommitError::NONE);
  std::move(on_commit_callback).Run(commit_erorr);
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
FakeSharingMessageBridge::GetControllerDelegate() {
  return nullptr;
}

SharingBrowserTest::SharingBrowserTest()
    : SyncTest(TWO_CLIENT),
      scoped_testing_factory_installer_(
          base::BindRepeating(&gcm::FakeGCMProfileService::Build)),
      sharing_service_(nullptr),
      fake_web_push_sender_(nullptr) {}

SharingBrowserTest::~SharingBrowserTest() = default;

void SharingBrowserTest::SetUpOnMainThread() {
  SyncTest::SetUpOnMainThread();
  host_resolver()->AddRule("mock.http", "127.0.0.1");
  host_resolver()->AddRule("mock2.http", "127.0.0.1");
}

void SharingBrowserTest::Init(
    sync_pb::SharingSpecificFields_EnabledFeatures first_device_feature,
    sync_pb::SharingSpecificFields_EnabledFeatures second_device_feature) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("mock.http", GetTestPageURL());
  ASSERT_TRUE(sessions_helper::OpenTab(0, url));

  web_contents_ = GetBrowser(0)->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(NavigateToURL(web_contents_, url));

  sharing_service_ = SharingServiceFactory::GetForBrowserContext(GetProfile(0));

  SharingFCMSender* sharing_fcm_sender =
      sharing_service_->GetMessageSenderForTesting()->GetFCMSenderForTesting();
  fake_web_push_sender_ = new FakeWebPushSender();
  sharing_fcm_sender->SetWebPushSenderForTesting(
      base::WrapUnique(fake_web_push_sender_));
  sharing_fcm_sender->SetSharingMessageBridgeForTesting(
      &fake_sharing_message_bridge_);

  SetUpDevices(first_device_feature, second_device_feature);
}

void SharingBrowserTest::SetUpDevices(
    sync_pb::SharingSpecificFields_EnabledFeatures first_device_feature,
    sync_pb::SharingSpecificFields_EnabledFeatures second_device_feature) {
  ASSERT_EQ(2u, GetSyncClients().size());

  RegisterDevice(0, first_device_feature);
  RegisterDevice(1, second_device_feature);

  syncer::DeviceInfoTracker* original_device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetDeviceInfoTracker();
  std::vector<std::unique_ptr<syncer::DeviceInfo>> original_devices =
      original_device_info_tracker->GetAllDeviceInfo();
  ASSERT_EQ(2u, original_devices.size());

  for (size_t i = 0; i < original_devices.size(); i++)
    AddDeviceInfo(*original_devices[i], i);
  ASSERT_EQ(2, fake_device_info_tracker_.CountActiveDevices());
}

void SharingBrowserTest::RegisterDevice(
    int profile_index,
    sync_pb::SharingSpecificFields_EnabledFeatures feature) {
  SharingService* service =
      SharingServiceFactory::GetForBrowserContext(GetProfile(profile_index));
  static_cast<SharingDeviceSourceSync*>(service->GetDeviceSource())
      ->SetDeviceInfoTrackerForTesting(&fake_device_info_tracker_);

  base::RunLoop run_loop;
  service->RegisterDeviceInTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures>{feature},
      base::BindLambdaForTesting([&](SharingDeviceRegistrationResult r) {
        ASSERT_EQ(SharingDeviceRegistrationResult::kSuccess, r);
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(AwaitQuiescence());
}

void SharingBrowserTest::AddDeviceInfo(
    const syncer::DeviceInfo& original_device,
    int fake_device_id) {
  std::unique_ptr<syncer::DeviceInfo> fake_device =
      std::make_unique<syncer::DeviceInfo>(
          original_device.guid(),
          base::StrCat(
              {"testing_device_", base::NumberToString(fake_device_id)}),
          original_device.chrome_version(), original_device.sync_user_agent(),
          original_device.device_type(),
          original_device.signin_scoped_device_id(),
          base::SysInfo::HardwareInfo{
              "Google",
              base::StrCat({"model", base::NumberToString(fake_device_id)}),
              "serial_number"},
          original_device.last_updated_timestamp(),
          original_device.pulse_interval(),
          original_device.send_tab_to_self_receiving_enabled(),
          original_device.sharing_info());
  fake_device_info_tracker_.Add(fake_device.get());
  device_infos_.push_back(std::move(fake_device));
}

std::unique_ptr<TestRenderViewContextMenu> SharingBrowserTest::InitContextMenu(
    const GURL& url,
    base::StringPiece link_text,
    base::StringPiece selection_text) {
  content::ContextMenuParams params;
  params.selection_text = base::ASCIIToUTF16(selection_text);
  params.media_type = blink::ContextMenuDataMediaType::kNone;
  params.unfiltered_link_url = url;
  params.link_url = url;
  params.src_url = url;
  params.link_text = base::ASCIIToUTF16(link_text);
  params.page_url = web_contents_->GetVisibleURL();
  params.source_type = ui::MenuSourceType::MENU_SOURCE_MOUSE;
#if defined(OS_MACOSX)
  params.writing_direction_default = 0;
  params.writing_direction_left_to_right = 0;
  params.writing_direction_right_to_left = 0;
#endif
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      web_contents_->GetMainFrame(), params);
  menu->Init();
  return menu;
}

void SharingBrowserTest::CheckLastReceiver(
    const syncer::DeviceInfo& device) const {
  auto fcm_configuration = GetFCMChannel(device);
  ASSERT_TRUE(fcm_configuration);

  if (base::FeatureList::IsEnabled(kSharingSendViaSync)) {
    EXPECT_EQ(fcm_configuration->sender_id_fcm_token(),
              fake_sharing_message_bridge_.specifics()
                  .channel_configuration()
                  .fcm()
                  .token());
  } else {
    EXPECT_EQ(fcm_configuration->vapid_fcm_token(),
              fake_web_push_sender_->fcm_token());
  }
}

chrome_browser_sharing::SharingMessage
SharingBrowserTest::GetLastSharingMessageSent() const {
  chrome_browser_sharing::SharingMessage sharing_message;

  if (base::FeatureList::IsEnabled(kSharingSendViaSync)) {
    sharing_message.ParseFromString(
        fake_sharing_message_bridge_.specifics().payload());
  } else {
    sharing_message.ParseFromString(fake_web_push_sender_->message().payload);
  }
  return sharing_message;
}

SharingService* SharingBrowserTest::sharing_service() const {
  return sharing_service_;
}

content::WebContents* SharingBrowserTest::web_contents() const {
  return web_contents_;
}

PageActionIconView* SharingBrowserTest::GetPageActionIconView(
    PageActionIconType type) {
  return BrowserView::GetBrowserViewForBrowser(GetBrowser(0))
      ->toolbar_button_provider()
      ->GetPageActionIconView(type);
}
