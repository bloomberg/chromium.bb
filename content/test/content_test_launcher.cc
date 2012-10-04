// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/test_suite.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/shell/shell_content_client.h"
#include "content/shell/shell_main_delegate.h"
#include "content/shell/shell_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "content/public/app/startup_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#include "ui/base/win/scoped_ole_initializer.h"
#endif  // defined(OS_WIN)

namespace content {

class ContentShellTestSuiteInitializer
    : public testing::EmptyTestEventListener {
 public:
  ContentShellTestSuiteInitializer() {
  }

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    DCHECK(!GetContentClient());
    content_client_.reset(new ShellContentClient);
    browser_content_client_.reset(new ShellContentBrowserClient());
    content_client_->set_browser_for_testing(browser_content_client_.get());
    SetContentClient(content_client_.get());
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    DCHECK_EQ(content_client_.get(), GetContentClient());
    browser_content_client_.reset();
    content_client_.reset();
    SetContentClient(NULL);
  }

 private:
  scoped_ptr<ShellContentClient> content_client_;
  scoped_ptr<ShellContentBrowserClient> browser_content_client_;

  DISALLOW_COPY_AND_ASSIGN(ContentShellTestSuiteInitializer);
};

class ContentBrowserTestSuite : public ContentTestSuiteBase {
 public:
  ContentBrowserTestSuite(int argc, char** argv)
      : ContentTestSuiteBase(argc, argv) {
  }
  virtual ~ContentBrowserTestSuite() {
  }

 protected:
  virtual void Initialize() OVERRIDE {
    ContentTestSuiteBase::Initialize();

    testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new ContentShellTestSuiteInitializer);
  }
  virtual void Shutdown() OVERRIDE {
    base::TestSuite::Shutdown();
  }

  virtual ContentClient* CreateClientForInitialization() OVERRIDE {
    return new ShellContentClient();
  }

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ContentBrowserTestSuite);
};

class ContentTestLauncherDelegate : public test_launcher::TestLauncherDelegate {
 public:
  ContentTestLauncherDelegate() {}
  virtual ~ContentTestLauncherDelegate() {}

  virtual std::string GetEmptyTestName() OVERRIDE {
    return std::string();
  }

  virtual int RunTestSuite(int argc, char** argv) OVERRIDE {
    return ContentBrowserTestSuite(argc, argv).Run();
  }

  virtual bool AdjustChildProcessCommandLine(
      CommandLine* command_line, const FilePath& temp_data_dir) OVERRIDE {
    command_line->AppendSwitchPath(switches::kContentShellDataPath,
                                   temp_data_dir);
    return true;
  }

 protected:
  virtual content::ContentMainDelegate* CreateContentMainDelegate() OVERRIDE {
    return new ShellMainDelegate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentTestLauncherDelegate);
};

}  // namespace content

int main(int argc, char** argv) {
  // Always use fake WebRTC devices in this binary since we want to be able
  // to test WebRTC even if we don't have any devices on the system.
  media_stream::MediaStreamManager::AlwaysUseFakeDevice();

  content::ContentTestLauncherDelegate launcher_delegate;
  return test_launcher::LaunchTests(&launcher_delegate, argc, argv);
}
