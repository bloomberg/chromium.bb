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
  using ManageProfileHandler::AllowJavascript;
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
    handler()->AllowJavascript();
    web_ui()->ClearTrackedCalls();
  }

  void VerifyIconList(const base::Value* value) {
    const base::ListValue* icons = nullptr;
    ASSERT_TRUE(value->GetAsList(&icons));

    // Expect a non-empty list of dictionaries containing non-empty strings for
    // profile avatar icon urls and labels.
    EXPECT_FALSE(icons->empty());
    for (size_t i = 0; i < icons->GetSize(); ++i) {
      const base::DictionaryValue* icon = nullptr;
      EXPECT_TRUE(icons->GetDictionary(i, &icon));
      std::string icon_url;
      EXPECT_TRUE(icon->GetString("url", &icon_url));
      EXPECT_FALSE(icon_url.empty());
      std::string icon_label;
      EXPECT_TRUE(icon->GetString("label", &icon_label));
      EXPECT_FALSE(icon_label.empty());
    }
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile() const { return profile_; }
  TestManageProfileHandler* handler() const { return handler_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;

  Profile* profile_;
  std::unique_ptr<TestManageProfileHandler> handler_;
};

TEST_F(ManageProfileHandlerTest, HandleSetProfileIconToGaiaAvatar) {
  handler()->HandleSetProfileIconToGaiaAvatar(nullptr);

  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar));
}

TEST_F(ManageProfileHandlerTest, HandleSetProfileIconToDefaultAvatar) {
  base::ListValue list_args;
  list_args.AppendString("chrome://theme/IDR_PROFILE_AVATAR_15");
  handler()->HandleSetProfileIconToDefaultAvatar(&list_args);

  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_EQ(15, pref_service->GetInteger(prefs::kProfileAvatarIndex));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar));
}

TEST_F(ManageProfileHandlerTest, HandleSetProfileName) {
  base::ListValue list_args;
  list_args.AppendString("New Profile Name");
  handler()->HandleSetProfileName(&list_args);

  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_EQ("New Profile Name", pref_service->GetString(prefs::kProfileName));
}

TEST_F(ManageProfileHandlerTest, HandleGetAvailableIcons) {
  base::ListValue list_args;
  list_args.AppendString("get-icons-callback-id");
  handler()->HandleGetAvailableIcons(&list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  std::string callback_id;
  ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
  EXPECT_EQ("get-icons-callback-id", callback_id);

  VerifyIconList(data.arg3());
}

TEST_F(ManageProfileHandlerTest, ProfileAvatarChangedWebUIEvent) {
  handler()->OnProfileAvatarChanged(base::FilePath());

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  std::string event_id;
  ASSERT_TRUE(data.arg1()->GetAsString(&event_id));
  EXPECT_EQ("available-icons-changed", event_id);

  VerifyIconList(data.arg2());
}

}  // namespace settings
