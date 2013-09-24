// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome {

namespace {

class MockInstantServiceObserver : public InstantServiceObserver {
 public:
  MOCK_METHOD0(DefaultSearchProviderChanged, void());
  MOCK_METHOD0(GoogleURLUpdated, void());
};

class MockWebContentsObserver : public content::WebContentsObserver {
 public:
  MOCK_METHOD1(WebContentsDestroyed, void(content::WebContents*));

  // Dumb override to make MSVC happy.
  void Observe_(content::WebContents* contents) {
    content::WebContentsObserver::Observe(contents);
  }

 protected:
  friend class InstantServiceTest;
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest,
      DispatchDefaultSearchProviderChanged);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, DispatchGoogleURLUpdated);
};

class InstantServiceTest : public InstantUnitTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
    InstantUnitTestBase::SetUp();

    instant_service_observer_.reset(new MockInstantServiceObserver());
    instant_service_->AddObserver(instant_service_observer_.get());

    instant_ntp_contents_observer_.reset(new MockWebContentsObserver());
    instant_ntp_contents_observer_->Observe_(
        instant_service_->GetNTPContents());
  }

  virtual void TearDown() OVERRIDE {
    instant_service_->RemoveObserver(instant_service_observer_.get());
    instant_ntp_contents_observer_->Observe_(NULL);
    InstantUnitTestBase::TearDown();
  }

  scoped_ptr<MockInstantServiceObserver> instant_service_observer_;
  scoped_ptr<MockWebContentsObserver> instant_ntp_contents_observer_;
};

TEST_F(InstantServiceTest, DispatchDefaultSearchProviderChanged) {
  EXPECT_CALL(*instant_service_observer_.get(), DefaultSearchProviderChanged())
      .Times(1);
  EXPECT_CALL(*instant_ntp_contents_observer_.get(),
              WebContentsDestroyed(instant_service_->GetNTPContents()))
      .Times(1);

  GURL ntp_url = instant_service_->GetNTPContents()->GetURL();
  const std::string& new_base_url = "https://bar.com/";
  SetDefaultSearchProvider(new_base_url);
  GURL new_ntp_url = instant_service_->GetNTPContents()->GetURL();
  EXPECT_NE(ntp_url, new_ntp_url);
  EXPECT_TRUE(StartsWithASCII(new_ntp_url.spec(), new_base_url, true));
}

TEST_F(InstantServiceTest, DispatchGoogleURLUpdated) {
  EXPECT_CALL(*instant_service_observer_.get(), GoogleURLUpdated()).Times(1);
  EXPECT_CALL(*instant_ntp_contents_observer_.get(),
              WebContentsDestroyed(instant_service_->GetNTPContents()))
      .Times(1);

  GURL ntp_url = instant_service_->GetNTPContents()->GetURL();
  const std::string& new_base_url = "https://www.google.es/";
  NotifyGoogleBaseURLUpdate(new_base_url);
  GURL new_ntp_url = instant_service_->GetNTPContents()->GetURL();
  EXPECT_NE(ntp_url, new_ntp_url);
  EXPECT_TRUE(StartsWithASCII(new_ntp_url.spec(), new_base_url, true));
}

}  // namespace

}  // namespace chrome
