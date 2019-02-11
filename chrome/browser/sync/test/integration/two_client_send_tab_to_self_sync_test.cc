// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class TwoClientSendTabToSelfSyncTest : public SyncTest {
 public:
  TwoClientSendTabToSelfSyncTest() : SyncTest(TWO_CLIENT) {
    scoped_list_.InitAndEnableFeature(switches::kSyncSendTabToSelf);
  }

  ~TwoClientSendTabToSelfSyncTest() override {}

 private:
  base::test::ScopedFeatureList scoped_list_;

  DISALLOW_COPY_AND_ASSIGN(TwoClientSendTabToSelfSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfSyncTest,
                       AddedUrlFoundWhenBothClientsAlreadySyncing) {
  const GURL kUrl("https://www.example.com");
  const std::string kTitle("example");
  const base::Time kTime = base::Time::FromDoubleT(0);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  send_tab_to_self::SendTabToSelfModel* model0 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  model0->AddEntry(kUrl, kTitle, kTime);

  send_tab_to_self::SendTabToSelfSyncService* service1 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1));

  EXPECT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service1, kUrl).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientSendTabToSelfSyncTest,
                       ModelsMatchAfterAddWhenBothClientsAlreadySyncing) {
  const GURL kGurl0("https://www.example0.com");
  const std::string kTitle0("example0");
  const base::Time kTime0 = base::Time::FromDoubleT(0);

  const GURL kGurl1("https://www.example1.com");
  const std::string kTitle1("example1");
  const base::Time kTime1 = base::Time::FromDoubleT(1);

  const GURL kGurl2("https://www.example2.com");
  const std::string kTitle2("example2");
  const base::Time kTime2 = base::Time::FromDoubleT(2);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  send_tab_to_self::SendTabToSelfModel* model0 =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  model0->AddEntry(kGurl0, kTitle0, kTime0);
  model0->AddEntry(kGurl1, kTitle1, kTime1);

  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1))
      ->GetSendTabToSelfModel()
      ->AddEntry(kGurl2, kTitle2, kTime2);

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfModelEqualityChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(1)),
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)))
                  .Wait());
}
