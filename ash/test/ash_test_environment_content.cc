// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_environment_content.h"

#include "ash/test/ash_test_views_delegate.h"
#include "ash/test/content/test_shell_content_state.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"

namespace ash {
namespace test {
namespace {

class AshTestViewsDelegateContent : public AshTestViewsDelegate {
 public:
  AshTestViewsDelegateContent() {}
  ~AshTestViewsDelegateContent() override {}

  // AshTestViewsDelegate:
  content::WebContents* CreateWebContents(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) override {
    return content::WebContentsTester::CreateTestWebContents(browser_context,
                                                             site_instance);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AshTestViewsDelegateContent);
};

}  // namespace

// static
std::unique_ptr<AshTestEnvironment> AshTestEnvironment::Create() {
  return base::MakeUnique<AshTestEnvironmentContent>();
}

// static
std::string AshTestEnvironment::Get100PercentResourceFileName() {
  return "ash_test_resources_with_content_100_percent.pak";
}

AshTestEnvironmentContent::AshTestEnvironmentContent()
    : thread_bundle_(base::MakeUnique<content::TestBrowserThreadBundle>()) {}

AshTestEnvironmentContent::~AshTestEnvironmentContent() {}

void AshTestEnvironmentContent::SetUp() {
  ShellContentState* content_state = content_state_;
  if (!content_state) {
    test_shell_content_state_ = new TestShellContentState;
    content_state = test_shell_content_state_;
  }
  ShellContentState::SetInstance(content_state);
}

void AshTestEnvironmentContent::TearDown() {
  ShellContentState::DestroyInstance();
}

base::SequencedWorkerPool* AshTestEnvironmentContent::GetBlockingPool() {
  return content::BrowserThread::GetBlockingPool();
}

std::unique_ptr<AshTestViewsDelegate>
AshTestEnvironmentContent::CreateViewsDelegate() {
  return base::MakeUnique<AshTestViewsDelegateContent>();
}

}  // namespace test
}  // namespace ash
