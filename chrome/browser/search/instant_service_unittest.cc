// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/common/render_messages.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class MockInstantServiceObserver : public InstantServiceObserver {
 public:
  MOCK_METHOD1(DefaultSearchProviderChanged, void(bool));
};

class InstantServiceTest : public InstantUnitTestBase {
 protected:
  void SetUp() override {
    InstantUnitTestBase::SetUp();

    instant_service_observer_.reset(new MockInstantServiceObserver());
    instant_service_->AddObserver(instant_service_observer_.get());
  }

  void TearDown() override {
    instant_service_->RemoveObserver(instant_service_observer_.get());
    InstantUnitTestBase::TearDown();
  }

  InstantSearchPrerenderer* GetInstantSearchPrerenderer() {
    return instant_service_->GetInstantSearchPrerenderer();
  }

  std::vector<InstantMostVisitedItem>& most_visited_items() {
    return instant_service_->most_visited_items_;
  }

  std::unique_ptr<MockInstantServiceObserver> instant_service_observer_;
};

class InstantServiceEnabledTest : public InstantServiceTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));
    InstantServiceTest::SetUp();
  }
};

TEST_F(InstantServiceEnabledTest, DispatchDefaultSearchProviderChanged) {
  EXPECT_CALL(*instant_service_observer_.get(),
              DefaultSearchProviderChanged(false)).Times(1);

  const std::string new_base_url = "https://bar.com/";
  SetUserSelectedDefaultSearchProvider(new_base_url);
}

TEST_F(InstantServiceTest, DontDispatchGoogleURLUpdatedForNonGoogleURLs) {
  EXPECT_CALL(*instant_service_observer_.get(),
              DefaultSearchProviderChanged(false)).Times(1);
  const std::string new_dsp_url = "https://bar.com/";
  SetUserSelectedDefaultSearchProvider(new_dsp_url);
  testing::Mock::VerifyAndClearExpectations(instant_service_observer_.get());

  EXPECT_CALL(*instant_service_observer_.get(),
              DefaultSearchProviderChanged(false)).Times(0);
  const std::string new_base_url = "https://www.google.es/";
  NotifyGoogleBaseURLUpdate(new_base_url);
}

TEST_F(InstantServiceTest, DispatchGoogleURLUpdated) {
  EXPECT_CALL(*instant_service_observer_.get(),
              DefaultSearchProviderChanged(true)).Times(1);

  const std::string new_base_url = "https://www.google.es/";
  NotifyGoogleBaseURLUpdate(new_base_url);
}

TEST_F(InstantServiceEnabledTest, SendsSearchURLsToRenderer) {
  std::unique_ptr<content::MockRenderProcessHost> rph(
      new content::MockRenderProcessHost(profile()));
  rph->sink().ClearMessages();
  instant_service_->Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::Source<content::MockRenderProcessHost>(rph.get()),
      content::NotificationService::NoDetails());
  EXPECT_EQ(1U, rph->sink().message_count());
  const IPC::Message* msg = rph->sink().GetMessageAt(0);
  ASSERT_TRUE(msg);
  ChromeViewMsg_SetSearchURLs::Param params;
  ChromeViewMsg_SetSearchURLs::Read(msg, &params);
  std::vector<GURL> search_urls = std::get<0>(params);
  GURL new_tab_page_url = std::get<1>(params);
  ASSERT_EQ(2U, search_urls.size());
  EXPECT_EQ("https://www.google.com/alt#quux=", search_urls[0].spec());
  EXPECT_EQ("https://www.google.com/url?bar=", search_urls[1].spec());
  EXPECT_EQ("https://www.google.com/newtab", new_tab_page_url.spec());
}

TEST_F(InstantServiceTest, InstantSearchEnabled) {
  EXPECT_NE(nullptr, GetInstantSearchPrerenderer());
}

TEST_F(InstantServiceEnabledTest,
       ResetInstantSearchPrerenderer_DefaultProviderChanged) {
  EXPECT_CALL(*instant_service_observer_.get(),
              DefaultSearchProviderChanged(false)).Times(2);

  // Set a default search provider that doesn't support Instant.
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("foobar.com"));
  data.SetURL("https://foobar.com/url?bar={searchTerms}");
  TemplateURL* template_url =
      template_url_service_->Add(base::MakeUnique<TemplateURL>(data));
  template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);

  EXPECT_EQ(nullptr, GetInstantSearchPrerenderer());

  // Set a default search provider that supports Instant and make sure
  // InstantSearchPrerenderer is valid.
  SetUserSelectedDefaultSearchProvider("https://google.com/");
  EXPECT_NE(nullptr, GetInstantSearchPrerenderer());
}

TEST_F(InstantServiceEnabledTest,
       ResetInstantSearchPrerenderer_GoogleBaseURLUpdated) {
  EXPECT_CALL(*instant_service_observer_.get(),
              DefaultSearchProviderChanged(true)).Times(1);

  InstantSearchPrerenderer* old_prerenderer = GetInstantSearchPrerenderer();
  ASSERT_NE(nullptr, old_prerenderer);

  const std::string new_base_url = "https://www.google.es/";
  NotifyGoogleBaseURLUpdate(new_base_url);
  EXPECT_NE(old_prerenderer, GetInstantSearchPrerenderer());
}

TEST_F(InstantServiceEnabledTest,
       ResetInstantSearchPrerenderer_InstantURLUpdated) {
  InstantSearchPrerenderer* old_prerenderer = GetInstantSearchPrerenderer();
  ASSERT_NE(nullptr, old_prerenderer);

  // Change the Instant URL *without* notifying the InstantService. That can
  // happen when some parameter (from UIThreadSearchTermsData) that goes into
  // the Instant URL is changed, e.g. the RLZ value (see crbug.com/660923). The
  // prerenderer should automatically get reset on the next access.
  const std::string new_base_url = "https://www.google.es/";
  UIThreadSearchTermsData::SetGoogleBaseURL(new_base_url);
  InstantSearchPrerenderer* new_prerenderer = GetInstantSearchPrerenderer();
  EXPECT_NE(old_prerenderer, new_prerenderer);

  // Make sure the next access does *not* reset the prerenderer again.
  EXPECT_EQ(new_prerenderer, GetInstantSearchPrerenderer());
}

TEST_F(InstantServiceTest, GetSuggestionFromClientSide) {
  history::MostVisitedURLList url_list;
  url_list.push_back(history::MostVisitedURL());

  instant_service_->OnTopSitesReceived(url_list);

  auto items = instant_service_->most_visited_items_;
  ASSERT_EQ(1, (int)items.size());
  ASSERT_EQ(ntp_tiles::NTPTileSource::TOP_SITES, items[0].source);
}
