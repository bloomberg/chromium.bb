// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_message_handler.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/sync/driver/about_sync_util.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestableSyncInternalsMessageHandler : public SyncInternalsMessageHandler {
 public:
  explicit TestableSyncInternalsMessageHandler(
      content::WebUI* web_ui,
      std::unique_ptr<AboutSyncDataExtractor> about_sync_data_extractor)
      : SyncInternalsMessageHandler(std::move(about_sync_data_extractor)) {
    set_web_ui(web_ui);
  }
};

class FakeExtractor : public AboutSyncDataExtractor {
 public:
  std::unique_ptr<base::DictionaryValue> ConstructAboutInformation(
      syncer::SyncService* service,
      SigninManagerBase* signin) override {
    call_count_++;
    last_service_ = service;
    last_signin_ = signin;
    std::unique_ptr<base::DictionaryValue> dictionary(
        new base::DictionaryValue());
    dictionary->SetString("fake_key", "fake_value");
    return dictionary;
  }

  int call_count() const { return call_count_; }
  syncer::SyncService* last_service() const { return last_service_; }
  SigninManagerBase* last_signin() const { return last_signin_; }

 private:
  int call_count_ = 0;
  syncer::SyncService* last_service_ = nullptr;
  SigninManagerBase* last_signin_ = nullptr;
};

class SyncInternalsMessageHandlerTest : public ::testing::Test {
 protected:
  SyncInternalsMessageHandlerTest() {
    site_instance_ = content::SiteInstance::Create(&profile_);
    web_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(&profile_, site_instance_.get())));
    web_ui_.set_web_contents(web_contents_.get());
    fake_extractor_ = new FakeExtractor();
    handler_.reset(new TestableSyncInternalsMessageHandler(
        &web_ui_, std::unique_ptr<FakeExtractor>(fake_extractor_)));
  }

  void ValidateAboutInfoCall() {
    const auto& data_vector = web_ui_.call_data();
    ASSERT_FALSE(data_vector.empty());
    EXPECT_EQ(1u, data_vector.size());

    const content::TestWebUI::CallData& call_data = *data_vector[0];

    EXPECT_EQ(syncer::sync_ui_util::kDispatchEvent, call_data.function_name());

    const base::Value* arg1 = call_data.arg1();
    ASSERT_TRUE(arg1);
    std::string event_type;
    EXPECT_TRUE(arg1->GetAsString(&event_type));
    EXPECT_EQ(syncer::sync_ui_util::kOnAboutInfoUpdated, event_type);

    const base::Value* arg2 = call_data.arg2();
    ASSERT_TRUE(arg2);

    const base::DictionaryValue* root_dictionary = nullptr;
    ASSERT_TRUE(arg2->GetAsDictionary(&root_dictionary));

    std::string fake_value;
    EXPECT_TRUE(root_dictionary->GetString("fake_key", &fake_value));
    EXPECT_EQ("fake_value", fake_value);
  }

  SyncInternalsMessageHandler* handler() { return handler_.get(); }
  FakeExtractor* fake_extractor() { return fake_extractor_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  scoped_refptr<content::SiteInstance> site_instance_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<SyncInternalsMessageHandler> handler_;

  // Non-owning pointer to the about information the handler uses. This
  // extractor is owned by the handler.
  FakeExtractor* fake_extractor_;
};

}  // namespace

TEST_F(SyncInternalsMessageHandlerTest, SendAboutInfoWithService) {
  handler()->OnStateChanged(nullptr);
  EXPECT_EQ(1, fake_extractor()->call_count());
  EXPECT_NE(nullptr, fake_extractor()->last_service());
  EXPECT_NE(nullptr, fake_extractor()->last_signin());
  ValidateAboutInfoCall();
}

TEST_F(SyncInternalsMessageHandlerTest, SendAboutInfoWithoutService) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableSync);
  handler()->OnStateChanged(nullptr);
  EXPECT_EQ(1, fake_extractor()->call_count());
  EXPECT_EQ(nullptr, fake_extractor()->last_service());
  EXPECT_EQ(nullptr, fake_extractor()->last_signin());
  ValidateAboutInfoCall();
}
