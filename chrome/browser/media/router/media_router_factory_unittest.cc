// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

namespace {

std::unique_ptr<KeyedService> CreateMockMediaRouter(
    content::BrowserContext* context) {
  return base::WrapUnique(new MockMediaRouter);
}

}  // namespace

class MediaRouterFactoryTest : public testing::Test {
 protected:
  MediaRouterFactoryTest() {}
  ~MediaRouterFactoryTest() override {}

  Profile* profile() { return &profile_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

TEST_F(MediaRouterFactoryTest, CreateForRegularProfile) {
  ASSERT_TRUE(MediaRouterFactory::GetApiForBrowserContext(profile()));
}

TEST_F(MediaRouterFactoryTest, CreateForOffTheRecordProfile) {
  Profile* otr_profile = profile()->GetOffTheRecordProfile();
  ASSERT_TRUE(otr_profile);

  // Makes sure a MediaRouter can be created from an OTR Profile.
  MediaRouter* router =
      MediaRouterFactory::GetApiForBrowserContext(otr_profile);
  ASSERT_TRUE(router);

  // A Profile and its OTR Profile share the same MediaRouter instance.
  ASSERT_EQ(router, MediaRouterFactory::GetApiForBrowserContext(profile()));
}

TEST_F(MediaRouterFactoryTest, OffTheRecordBrowserContextShutdown) {
  MediaRouterFactory::GetMediaRouterFactoryForTest()->SetTestingFactory(
      profile(), &CreateMockMediaRouter);

  // Creates an off the record profile.
  profile()->GetOffTheRecordProfile();
  MockMediaRouter* router = static_cast<MockMediaRouter*>(
      MediaRouterFactory::GetApiForBrowserContext(profile()));
  ASSERT_TRUE(router);
  EXPECT_CALL(*router, OnOffTheRecordProfileShutdown());
  profile()->DestroyOffTheRecordProfile();
}

}  // namespace media_router
