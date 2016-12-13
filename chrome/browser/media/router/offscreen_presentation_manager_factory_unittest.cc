// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/media/router/offscreen_presentation_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class OffscreenPresentationManagerFactoryTest : public testing::Test {
 protected:
  OffscreenPresentationManagerFactoryTest() {}
  ~OffscreenPresentationManagerFactoryTest() override {}

  Profile* profile() { return &profile_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

TEST_F(OffscreenPresentationManagerFactoryTest, CreateForRegularProfile) {
  ASSERT_TRUE(OffscreenPresentationManagerFactory::GetOrCreateForBrowserContext(
      profile()));
}

TEST_F(OffscreenPresentationManagerFactoryTest, CreateForOffTheRecordProfile) {
  Profile* incognito_profile = profile()->GetOffTheRecordProfile();
  ASSERT_TRUE(incognito_profile);

  // Makes sure a OffscreenPresentationManager can be created from an incognito
  // Profile.
  OffscreenPresentationManager* manager =
      OffscreenPresentationManagerFactory::GetOrCreateForBrowserContext(
          incognito_profile);
  ASSERT_TRUE(manager);

  // A Profile and its incognito Profile share the same
  // OffscreenPresentationManager instance.
  ASSERT_EQ(manager,
            OffscreenPresentationManagerFactory::GetOrCreateForBrowserContext(
                profile()));
}

}  // namespace media_router
