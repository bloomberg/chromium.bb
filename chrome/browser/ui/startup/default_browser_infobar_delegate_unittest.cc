// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_infobar_delegate.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

// An implementation of DefaultBrowserWorker that will simply invoke the
// 'on_finished_callback' immediately.
class FakeDefaultBrowserWorker
    : public shell_integration::DefaultBrowserWorker {
 public:
  FakeDefaultBrowserWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback)
      : shell_integration::DefaultBrowserWorker(callback) {}

 private:
  ~FakeDefaultBrowserWorker() override = default;

  shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
    return shell_integration::NOT_DEFAULT;
  }

  void SetAsDefaultImpl(const base::Closure& on_finished_callback) override {
    on_finished_callback.Run();
  }

  DISALLOW_COPY_AND_ASSIGN(FakeDefaultBrowserWorker);
};

class FakeDefaultBrowserInfoBarDelegate : public DefaultBrowserInfoBarDelegate {
 public:
  FakeDefaultBrowserInfoBarDelegate()
      : DefaultBrowserInfoBarDelegate(nullptr) {}

 private:
  scoped_refptr<shell_integration::DefaultBrowserWorker>
  CreateDefaultBrowserWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback)
      override {
    return new FakeDefaultBrowserWorker(callback);
  }

  DISALLOW_COPY_AND_ASSIGN(FakeDefaultBrowserInfoBarDelegate);
};

class DefaultBrowserInfoBarDelegateTest : public ::testing::Test {
 public:
  DefaultBrowserInfoBarDelegateTest()
      : profile_(new TestingProfile()),
        web_contents_(factory_.CreateWebContents(profile_.get())) {
    InfoBarService::CreateForWebContents(web_contents_);
    infobar_service_ = InfoBarService::FromWebContents(web_contents_);
  }

 protected:
  void EnableStickyDefaultBrowserPrompt() {
    scoped_feature_list_.InitAndEnableFeature(kStickyDefaultBrowserPrompt);
  }

  void AddDefaultBrowserInfoBar() {
    infobar_service_->AddInfoBar(infobar_service_->CreateConfirmInfoBar(
        base::MakeUnique<FakeDefaultBrowserInfoBarDelegate>()));
  }

  InfoBarService* infobar_service() { return infobar_service_; }

 private:
  // The DefaultBrowserWorker requires a FILE thread. Also provides a
  // SingleThreadTaskRunner for the test profile and the default browser prompt.
  content::TestBrowserThreadBundle thread_bundle;

  // Required to get an InfoBarService.
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory factory_;
  content::WebContents* web_contents_;

  // Manages the default browser prompt.
  InfoBarService* infobar_service_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DefaultBrowserInfoBarDelegateTest);
};

TEST_F(DefaultBrowserInfoBarDelegateTest, DefaultBehavior) {
  AddDefaultBrowserInfoBar();
  ASSERT_EQ(1U, infobar_service()->infobar_count());

  infobars::InfoBar* default_browser_infobar = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* default_browser_infobar_delegate =
      default_browser_infobar->delegate()->AsConfirmInfoBarDelegate();

  // When the sticky default browser prompt experiment is not enabled, the
  // infobar delegate should allow itself to be destroyed immediately if the
  // user activates the default action. Accept() should return true to indicate
  // this.
  EXPECT_TRUE(default_browser_infobar_delegate->Accept());
}

#if defined(OS_WIN)
TEST_F(DefaultBrowserInfoBarDelegateTest, StickyDefaultBrowserPrompt) {
  EnableStickyDefaultBrowserPrompt();

  // For most Windows versions, this experiment is disabled.
  if (!IsStickyDefaultBrowserPromptEnabled())
    return;

  AddDefaultBrowserInfoBar();
  ASSERT_EQ(1U, infobar_service()->infobar_count());

  infobars::InfoBar* default_browser_infobar = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* default_browser_infobar_delegate =
      default_browser_infobar->delegate()->AsConfirmInfoBarDelegate();

  // The sticky default browser prompt experiment should mean activating the
  // infobar's default action does not result in the infobar's destruction.
  // Instead, the infobar will ultimately be destroyed when the
  // DefaultBrowserWorker is done. To indicate this Accept() should return
  // false.
  EXPECT_FALSE(default_browser_infobar_delegate->Accept());
  EXPECT_EQ(1U, infobar_service()->infobar_count());

  // Spin the message loop to allow the FakeDefaultBrowserWorker to complete,
  // which should destroy the infobar.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0U, infobar_service()->infobar_count());
}
#endif  // defined(OS_WIN)

}  // namespace chrome
