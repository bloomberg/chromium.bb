// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/profile_info_handler.h"

#include <memory>

#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "components/user_manager/fake_user_manager.h"
#endif

namespace settings {

namespace {

class TestProfileInfoHandler : public ProfileInfoHandler {
 public:
  explicit TestProfileInfoHandler(Profile* profile)
      : ProfileInfoHandler(profile) {}

  using ProfileInfoHandler::set_web_ui;
};

}  // namespace

class ProfileInfoHandlerTest : public testing::Test {
 public:
  ProfileInfoHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
#if defined(OS_CHROMEOS)
        user_manager_(new user_manager::FakeUserManager),
        user_manager_enabler_(user_manager_),
#endif
        profile_(nullptr) {
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

#if defined(OS_CHROMEOS)
    profile_ = profile_manager_.CreateTestingProfile("fake_id@gmail.com");
#else
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");
#endif

    handler_.reset(new TestProfileInfoHandler(profile_));
    handler_->set_web_ui(&web_ui_);
  }

  void VerifyResponse(const base::Value* call_argument) {
    const base::DictionaryValue* response = nullptr;
    ASSERT_TRUE(call_argument->GetAsDictionary(&response));

    std::string name;
    std::string icon_url;
    ASSERT_TRUE(response->GetString("name", &name));
    ASSERT_TRUE(response->GetString("iconUrl", &icon_url));

#if defined(OS_CHROMEOS)
    EXPECT_EQ("fake_id@gmail.com", name);
    EXPECT_FALSE(icon_url.empty());
#else
    EXPECT_EQ("Profile 1", name);
    EXPECT_EQ("chrome://theme/IDR_PROFILE_AVATAR_0", icon_url);
#endif
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile() const { return profile_; }
  TestProfileInfoHandler* handler() const { return handler_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;

#if defined(OS_CHROMEOS)
  // |user_manager_| is destroyed by |user_manager_enabler_|.
  user_manager::FakeUserManager* user_manager_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
#endif

  Profile* profile_;
  std::unique_ptr<TestProfileInfoHandler> handler_;
};

TEST_F(ProfileInfoHandlerTest, GetProfileInfo) {
  base::ListValue list_args;
  list_args.Append(new base::StringValue("get-profile-info-callback-id"));
  handler()->HandleGetProfileInfo(&list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  std::string callback_id;
  ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
  EXPECT_EQ("get-profile-info-callback-id", callback_id);

  bool success = false;
  ASSERT_TRUE(data.arg2()->GetAsBoolean(&success));
  EXPECT_TRUE(success);

  VerifyResponse(data.arg3());
}

TEST_F(ProfileInfoHandlerTest, PushProfileInfo) {
  base::ListValue list_args;
  list_args.Append(new base::StringValue("get-profile-info-callback-id"));
  handler()->HandleGetProfileInfo(&list_args);

  handler()->OnProfileAvatarChanged(base::FilePath());

  EXPECT_EQ(2U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  std::string event_id;
  ASSERT_TRUE(data.arg1()->GetAsString(&event_id));
  EXPECT_EQ(ProfileInfoHandler::kProfileInfoChangedEventName, event_id);

  VerifyResponse(data.arg2());
}

}  // namespace settings
