// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

class SingleClientSendTabToSelfSyncTest : public SyncTest {
 public:
  SingleClientSendTabToSelfSyncTest() : SyncTest(SINGLE_CLIENT) {
    scoped_list_.InitAndEnableFeature(switches::kSyncSendTabToSelf);
  }

  ~SingleClientSendTabToSelfSyncTest() override {}

 private:
  base::test::ScopedFeatureList scoped_list_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientSendTabToSelfSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest,
                       DownloadWhenSyncEnabled) {
  const std::string kUrl("https://www.example.com");
  const std::string kGuid("kGuid");
  sync_pb::EntitySpecifics specifics;
  sync_pb::SendTabToSelfSpecifics* send_tab_to_self =
      specifics.mutable_send_tab_to_self();
  send_tab_to_self->set_url(kUrl);
  send_tab_to_self->set_guid(kGuid);
  send_tab_to_self->set_shared_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kGuid, specifics,
          /*creation_time=*/base::Time::Now().ToTimeT(),
          /*last_modified_time=*/base::Time::Now().ToTimeT()));

  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfUrlChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)),
                  GURL(kUrl))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest, IsActive) {
  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfActiveChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)))
                  .Wait());
  EXPECT_TRUE(send_tab_to_self::IsUserSyncTypeActive(GetProfile(0)));
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest, IsOnMultipleDevices) {
  ASSERT_TRUE(SetupSync());

  EXPECT_FALSE(send_tab_to_self::IsSyncingOnMultipleDevices(GetProfile(0)));
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest, IsFlagEnabled) {
  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(send_tab_to_self::IsReceivingEnabled());
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest, ShouldOfferFeature) {
  ASSERT_TRUE(SetupSync());

  EXPECT_FALSE(send_tab_to_self::ShouldOfferFeature(
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents()));
}

}  // namespace
