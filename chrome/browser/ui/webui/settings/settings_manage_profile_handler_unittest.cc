// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings {

namespace {

class TestManageProfileHandler : public ManageProfileHandler {
 public:
  explicit TestManageProfileHandler(Profile* profile)
      : ManageProfileHandler(profile) {}

  using ManageProfileHandler::set_web_ui;
};

}  // namespace

class ManageProfileHandlerTest : public testing::Test {
 public:
  ManageProfileHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_(nullptr) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");

    handler_.reset(new TestManageProfileHandler(profile_));
    handler_->set_web_ui(&web_ui_);
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile() const { return profile_; }
  TestManageProfileHandler* handler() const { return handler_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;

  Profile* profile_;
  scoped_ptr<TestManageProfileHandler> handler_;
};

TEST_F(ManageProfileHandlerTest, SetProfileIconAndName) {
  base::ListValue list_args;
  list_args.Append(
      new base::StringValue("chrome://theme/IDR_PROFILE_AVATAR_15"));
  list_args.Append(new base::StringValue("New Profile Name"));
  handler()->HandleSetProfileIconAndName(&list_args);

  PrefService* pref_service = profile()->GetPrefs();

  EXPECT_EQ(15, pref_service->GetInteger(prefs::kProfileAvatarIndex));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar));
  EXPECT_EQ("New Profile Name", pref_service->GetString(prefs::kProfileName));
}

TEST_F(ManageProfileHandlerTest, GetAvailableIcons) {
  handler()->HandleGetAvailableIcons(nullptr);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  std::string callback_id;
  ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
  EXPECT_EQ("available-icons-changed", callback_id);

  const base::ListValue* icon_urls = nullptr;
  ASSERT_TRUE(data.arg2()->GetAsList(&icon_urls));

  // Expect the list of icon URLs to be a non-empty list of non-empty strings.
  EXPECT_FALSE(icon_urls->empty());
  for (size_t i = 0; i < icon_urls->GetSize(); ++i) {
    std::string icon_url;
    EXPECT_TRUE(icon_urls->GetString(i, &icon_url));
    EXPECT_FALSE(icon_url.empty());
  }
}

}  // namespace settings
