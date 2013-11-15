// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/render_messages.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

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
    InstantUnitTestBase::SetUpWithoutCacheableNTP();

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

TEST_F(InstantServiceTest, SendsSearchURLsToRenderer) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
      "Group1 use_cacheable_ntp:1"));
  scoped_ptr<content::MockRenderProcessHost> rph(
      new content::MockRenderProcessHost(profile()));
  rph->sink().ClearMessages();
  instant_service_->Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::Source<content::MockRenderProcessHost>(rph.get()),
      content::NotificationService::NoDetails());
  EXPECT_EQ(1U, rph->sink().message_count());
  const IPC::Message* msg = rph->sink().GetMessageAt(0);
  ASSERT_TRUE(msg);
  std::vector<GURL> search_urls;
  GURL new_tab_page_url;
  ChromeViewMsg_SetSearchURLs::Read(msg, &search_urls, &new_tab_page_url);
  EXPECT_EQ(2U, search_urls.size());
  EXPECT_EQ("https://www.google.com/alt#quux=", search_urls[0].spec());
  EXPECT_EQ("https://www.google.com/url?bar=", search_urls[1].spec());
  EXPECT_EQ("https://www.google.com/newtab", new_tab_page_url.spec());
}
