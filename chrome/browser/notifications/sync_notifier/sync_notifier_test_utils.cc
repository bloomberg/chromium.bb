// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/sync_notifier_test_utils.h"

// Test data for App Info structures.
const char kSendingService1Name[] = "TestService1";
const char kSendingService2Name[] = "TestService2";
const char kSendingService3Name[] = "TestService3";
const char kTestIconUrl[] = "https://www.google.com/someicon.png";

// Fake data for creating a SyncData object to use in creating a
// SyncedNotification.
const char kAppId1[] = "fboilmbenheemaomgaeehigklolhkhnf";
const char kAppId2[] = "fbcmoldooppoahjhfflnmljoanccekpf";
const char kAppId3[] = "fbcmoldooppoahjhfflnmljoanccek33";
const char kAppId4[] = "fbcmoldooppoahjhfflnmljoanccek44";
const char kAppId5[] = "fbcmoldooppoahjhfflnmljoanccek55";
const char kAppId6[] = "fbcmoldooppoahjhfflnmljoanccek66";
const char kAppId7[] = "fbcmoldooppoahjhfflnmljoanccek77";
const char kKey1[] = "foo";
const char kKey2[] = "bar";
const char kKey3[] = "bat";
const char kKey4[] = "baz";
const char kKey5[] = "foobar";
const char kKey6[] = "fu";
const char kKey7[] = "meta";
const char kIconUrl1[] = "http://www.google.com/icon1.jpg";
const char kIconUrl2[] = "http://www.google.com/icon2.jpg";
const char kIconUrl3[] = "http://www.google.com/icon3.jpg";
const char kIconUrl4[] = "http://www.google.com/icon4.jpg";
const char kIconUrl5[] = "http://www.google.com/icon5.jpg";
const char kIconUrl6[] = "http://www.google.com/icon6.jpg";
const char kIconUrl7[] = "http://www.google.com/icon7.jpg";
const char kTitle1[] = "New appointment at 2:15";
const char kTitle2[] = "Email from Mark: Upcoming Ski trip";
const char kTitle3[] = "Weather alert - light rain tonight.";
const char kTitle4[] = "Zombie Alert on I-405";
const char kTitle5[] = "5-dimensional plutonian steam hockey scores";
const char kTitle6[] = "Conterfactuals Inc Stock report";
const char kTitle7[] = "Push Messaging app updated";
const char kText1[] = "Space Needle, 12:00 pm";
const char kText2[] = "Stevens Pass is our first choice.";
const char kText3[] = "More rain expected in the Seattle area tonight.";
const char kText4[] = "Traffic slowdown as motorists are hitting zombies";
const char kText5[] = "Neptune wins, pi to e";
const char kText6[] = "Beef flavored base for soups";
const char kText7[] = "You now have the latest version of Push Messaging App.";
const char kText1And1[] = "Space Needle, 12:00 pm\nSpace Needle, 12:00 pm";
const char kImageUrl1[] = "http://www.google.com/image1.jpg";
const char kImageUrl2[] = "http://www.google.com/image2.jpg";
const char kImageUrl3[] = "http://www.google.com/image3.jpg";
const char kImageUrl4[] = "http://www.google.com/image4.jpg";
const char kImageUrl5[] = "http://www.google.com/image5.jpg";
const char kImageUrl6[] = "http://www.google.com/image6.jpg";
const char kImageUrl7[] = "http://www.google.com/image7.jpg";
const char kExpectedOriginUrl[] =
    "synced-notification://fboilmbenheemaomgaeehigklolhkhnf";
const char kDefaultDestinationTitle[] = "Open web page";
const char kDefaultDestinationIconUrl[] = "http://www.google.com/image4.jpg";
const char kDefaultDestinationUrl[] = "chrome://flags";
const char kButtonOneTitle[] = "Read";
const char kButtonOneIconUrl[] = "http://www.google.com/image8.jpg";
const char kButtonOneUrl[] = "chrome://sync";
const char kButtonTwoTitle[] = "Reply";
const char kButtonTwoIconUrl[] = "http://www.google.com/image9.jpg";
const char kButtonTwoUrl[] = "chrome://about";
const char kContainedTitle1[] = "Today's Picnic moved";
const char kContainedTitle2[] = "Group Run Today";
const char kContainedTitle3[] = "Starcraft Tonight";
const char kContainedMessage1[] = "Due to rain, we will be inside the cafe.";
const char kContainedMessage2[] = "Meet at noon in the Gym.";
const char kContainedMessage3[] = "Let's play starcraft tonight on the LAN.";

syncer::SyncData CreateSyncData(
    const std::string& title,
    const std::string& text,
    const std::string& app_icon_url,
    const std::string& image_url,
    const std::string& app_id,
    const std::string& key,
    const sync_pb::CoalescedSyncedNotification_ReadState read_state) {
  // CreateLocalData makes a copy of this, so this can safely live
  // on the stack.
  sync_pb::EntitySpecifics entity_specifics;

  // Get a writeable pointer to the sync notifications specifics inside the
  // entity specifics.
  sync_pb::SyncedNotificationSpecifics* specifics =
      entity_specifics.mutable_synced_notification();

  // Get pointers to sub structures.
  sync_pb::CoalescedSyncedNotification* coalesced_notification =
      specifics->mutable_coalesced_notification();
  sync_pb::SyncedNotificationRenderInfo* render_info =
      coalesced_notification->mutable_render_info();
  sync_pb::ExpandedInfo* expanded_info =
      render_info->mutable_expanded_info();
  sync_pb::SimpleExpandedLayout* simple_expanded_layout =
      expanded_info->mutable_simple_expanded_layout();
  sync_pb::CollapsedInfo* collapsed_info =
      render_info->mutable_collapsed_info();
  sync_pb::SimpleCollapsedLayout* simple_collapsed_layout =
      collapsed_info->mutable_simple_collapsed_layout();
  sync_pb::SyncedNotificationDestination* default_destination =
      collapsed_info->mutable_default_destination();

  coalesced_notification->set_app_id(app_id);

  coalesced_notification->set_key(key);

  coalesced_notification->
      set_priority(static_cast<sync_pb::CoalescedSyncedNotification_Priority>(
          kProtobufPriority));

  // Set the title.
  simple_expanded_layout->set_title(title);
  simple_collapsed_layout->set_heading(title);

  // Set the text.
  simple_expanded_layout->set_text(text);
  simple_collapsed_layout->set_description(text);
  simple_collapsed_layout->set_annotation(text);

  // Set the heading.
  simple_collapsed_layout->set_heading(title);

  // Add the collapsed info and set the app_icon_url on it.
  simple_collapsed_layout->
      mutable_app_icon()->
      set_url(app_icon_url);

  // Add the media object and set the image url on it.
  simple_collapsed_layout->add_media();
  simple_collapsed_layout->
      mutable_media(0)->
      mutable_image()->
      set_url(image_url);

  coalesced_notification->set_creation_time_msec(kFakeCreationTime);

  coalesced_notification->set_read_state(read_state);

  // Contained notification one.
  expanded_info->add_collapsed_info();
  sync_pb::SimpleCollapsedLayout* notification_layout1 =
      expanded_info->
      mutable_collapsed_info(0)->
      mutable_simple_collapsed_layout();
  notification_layout1->set_heading(kContainedTitle1);
  notification_layout1->set_description(kContainedMessage1);

  // Contained notification two.
  expanded_info->add_collapsed_info();
  sync_pb::SimpleCollapsedLayout* notification_layout2 =
      expanded_info->
      mutable_collapsed_info(1)->
      mutable_simple_collapsed_layout();
  notification_layout2->set_heading(kContainedTitle2);
  notification_layout2->set_description(kContainedMessage2);

  // Contained notification three.
  expanded_info->add_collapsed_info();
  sync_pb::SimpleCollapsedLayout* notification_layout3 =
      expanded_info->
      mutable_collapsed_info(2)->
      mutable_simple_collapsed_layout();
  notification_layout3->set_heading(kContainedTitle3);
  notification_layout3->set_description(kContainedMessage3);

  // Default Destination.
  default_destination->set_text(kDefaultDestinationTitle);
  default_destination->mutable_icon()->set_url(kDefaultDestinationIconUrl);
  default_destination->mutable_icon()->set_alt_text(kDefaultDestinationTitle);
  default_destination->set_url(kDefaultDestinationUrl);

  // Buttons are represented as targets.

  // Button One.
  collapsed_info->add_target();
  sync_pb::SyncedNotificationAction* action1 =
      collapsed_info->mutable_target(0)->mutable_action();
  action1->set_text(kButtonOneTitle);
  action1->mutable_icon()->set_url(kButtonOneIconUrl);
  action1->mutable_icon()->set_alt_text(kButtonOneTitle);
  action1->set_url(kButtonOneUrl);

  // Button Two.
  collapsed_info->add_target();
  sync_pb::SyncedNotificationAction* action2 =
      collapsed_info->mutable_target(1)->mutable_action();
  action2->set_text(kButtonOneTitle);
  action2->mutable_icon()->set_url(kButtonTwoIconUrl);
  action2->mutable_icon()->set_alt_text(kButtonTwoTitle);
  action2->set_url(kButtonTwoUrl);

  syncer::SyncData sync_data = syncer::SyncData::CreateLocalData(
      "syncer::SYNCED_NOTIFICATIONS",
      "ChromeNotifierServiceUnitTest",
      entity_specifics);

  return sync_data;
}

namespace notifier {

StubSyncedNotificationAppInfoService::StubSyncedNotificationAppInfoService(
    Profile* profile) : SyncedNotificationAppInfoService(profile) {
  on_bitmap_fetches_done_called_ = false;
}

StubSyncedNotificationAppInfoService::~StubSyncedNotificationAppInfoService() {
}

syncer::SyncMergeResult
StubSyncedNotificationAppInfoService::MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) {
    return syncer::SyncMergeResult(syncer::SYNCED_NOTIFICATION_APP_INFO);
}

syncer::SyncError StubSyncedNotificationAppInfoService::ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) {
    return syncer::SyncError();
}

syncer::SyncDataList StubSyncedNotificationAppInfoService::GetAllSyncData(
    syncer::ModelType type) const {
    return syncer::SyncDataList();
}

void StubSyncedNotificationAppInfoService::OnBitmapFetchesDone(
    std::vector<std::string> added_app_ids,
    std::vector<std::string> removed_app_ids) {
    added_app_ids_ = added_app_ids;
    removed_app_ids_ = removed_app_ids;
    on_bitmap_fetches_done_called_ = true;
}

scoped_ptr<SyncedNotificationAppInfo>
StubSyncedNotificationAppInfoService::
    CreateSyncedNotificationAppInfoFromProtobuf(
        const sync_pb::SyncedNotificationAppInfo& app_info) {
  return scoped_ptr<SyncedNotificationAppInfo>();
}

SyncedNotificationAppInfo*
StubSyncedNotificationAppInfoService::FindSyncedNotificationAppInfoByName(
    const std::string& name) {
  return NULL;
}

SyncedNotificationAppInfo*
StubSyncedNotificationAppInfoService::FindSyncedNotificationAppInfoByAppId(
    const std::string& app_id) {
  return NULL;
}

std::string
StubSyncedNotificationAppInfoService::FindSendingServiceNameFromAppId(
    const std::string app_id) {
  return std::string();
}
std::vector<SyncedNotificationSendingServiceSettingsData>
StubSyncedNotificationAppInfoService::GetAllSendingServiceSettingsData() {
  std::vector<SyncedNotificationSendingServiceSettingsData> empty;
  return empty;
}


// Probe functions to return data.
std::vector<std::string> StubSyncedNotificationAppInfoService::added_app_ids() {
  return added_app_ids_;
}

std::vector<std::string>
StubSyncedNotificationAppInfoService::removed_app_ids() {
    return removed_app_ids_;
}
bool StubSyncedNotificationAppInfoService::on_bitmap_fetches_done_called() {
  return on_bitmap_fetches_done_called_;
}

}  // namespace notifier
