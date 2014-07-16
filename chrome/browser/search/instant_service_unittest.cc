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
  MOCK_METHOD0(DefaultSearchProviderChanged, void());
  MOCK_METHOD1(OmniboxStartMarginChanged, void(int));
};

class InstantServiceTest : public InstantUnitTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
    InstantUnitTestBase::SetUp();

    instant_service_observer_.reset(new MockInstantServiceObserver());
    instant_service_->AddObserver(instant_service_observer_.get());
  }

  virtual void TearDown() OVERRIDE {
    instant_service_->RemoveObserver(instant_service_observer_.get());
    InstantUnitTestBase::TearDown();
  }

  InstantSearchPrerenderer* GetInstantSearchPrerenderer() {
    return instant_service_->instant_search_prerenderer();
  }

  void UpdateOmniboxStartMargin(int start_margin) {
    instant_service_->OnOmniboxStartMarginChanged(start_margin);
  }

  scoped_ptr<MockInstantServiceObserver> instant_service_observer_;
};

class InstantServiceEnabledTest : public InstantServiceTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));
    InstantServiceTest::SetUp();
  }
};

TEST_F(InstantServiceEnabledTest, DispatchDefaultSearchProviderChanged) {
  EXPECT_CALL(*instant_service_observer_.get(), DefaultSearchProviderChanged())
      .Times(1);

  const std::string new_base_url = "https://bar.com/";
  SetUserSelectedDefaultSearchProvider(new_base_url);
}

TEST_F(InstantServiceTest, DontDispatchGoogleURLUpdatedForNonGoogleURLs) {
  EXPECT_CALL(*instant_service_observer_.get(), DefaultSearchProviderChanged())
      .Times(1);
  const std::string new_dsp_url = "https://bar.com/";
  SetUserSelectedDefaultSearchProvider(new_dsp_url);
  testing::Mock::VerifyAndClearExpectations(instant_service_observer_.get());

  EXPECT_CALL(*instant_service_observer_.get(), DefaultSearchProviderChanged())
      .Times(0);
  const std::string new_base_url = "https://www.google.es/";
  NotifyGoogleBaseURLUpdate(new_base_url);
}

TEST_F(InstantServiceTest, DispatchGoogleURLUpdated) {
  EXPECT_CALL(*instant_service_observer_.get(), DefaultSearchProviderChanged())
      .Times(1);

  const std::string new_base_url = "https://www.google.es/";
  NotifyGoogleBaseURLUpdate(new_base_url);
}

TEST_F(InstantServiceEnabledTest, SendsSearchURLsToRenderer) {
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
  ChromeViewMsg_SetSearchURLs::Param params;
  ChromeViewMsg_SetSearchURLs::Read(msg, &params);
  std::vector<GURL> search_urls = params.a;
  GURL new_tab_page_url = params.b;
  EXPECT_EQ(2U, search_urls.size());
  EXPECT_EQ("https://www.google.com/alt#quux=", search_urls[0].spec());
  EXPECT_EQ("https://www.google.com/url?bar=", search_urls[1].spec());
  EXPECT_EQ("https://www.google.com/newtab", new_tab_page_url.spec());
}

TEST_F(InstantServiceTest, InstantSearchEnabled) {
  EXPECT_NE(static_cast<InstantSearchPrerenderer*>(NULL),
            GetInstantSearchPrerenderer());
}

TEST_F(InstantServiceEnabledTest,
       ResetInstantSearchPrerenderer_DefaultProviderChanged) {
  EXPECT_CALL(*instant_service_observer_.get(), DefaultSearchProviderChanged())
      .Times(2);

  // Set a default search provider that doesn't support Instant.
  TemplateURLData data;
  data.SetURL("https://foobar.com/url?bar={searchTerms}");
  TemplateURL* template_url = new TemplateURL(data);
  // Takes ownership of |template_url|.
  template_url_service_->Add(template_url);
  template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);

  EXPECT_EQ(static_cast<InstantSearchPrerenderer*>(NULL),
            GetInstantSearchPrerenderer());

  // Set a default search provider that supports Instant and make sure
  // InstantSearchPrerenderer is valid.
  SetUserSelectedDefaultSearchProvider("https://google.com/");
  EXPECT_NE(static_cast<InstantSearchPrerenderer*>(NULL),
            GetInstantSearchPrerenderer());
}

TEST_F(InstantServiceEnabledTest,
       ResetInstantSearchPrerenderer_GoogleBaseURLUpdated) {
  EXPECT_CALL(*instant_service_observer_.get(), DefaultSearchProviderChanged())
      .Times(1);

  InstantSearchPrerenderer* old_prerenderer = GetInstantSearchPrerenderer();
  EXPECT_TRUE(old_prerenderer != NULL);

  const std::string new_base_url = "https://www.google.es/";
  NotifyGoogleBaseURLUpdate(new_base_url);
  EXPECT_NE(old_prerenderer, GetInstantSearchPrerenderer());
}

TEST_F(InstantServiceTest, OmniboxStartMarginChanged) {
  int new_start_margin = 92;
  EXPECT_CALL(*instant_service_observer_.get(),
              OmniboxStartMarginChanged(new_start_margin)).Times(1);
  UpdateOmniboxStartMargin(new_start_margin);
}
