// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/sys_info.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/shell/app/shell_main_delegate.h"
#include "content/shell/common/shell_switches.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/message_loop/message_loop.h"
#include "content/app/mojo/mojo_init.h"
#include "content/common/url_schemes.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/nested_message_pump_android.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/shell_content_client.h"
#include "ui/base/ui_base_paths.h"
#endif

namespace content {

#if defined(OS_ANDROID)
scoped_ptr<base::MessagePump> CreateMessagePumpForUI() {
  return scoped_ptr<base::MessagePump>(new NestedMessagePumpAndroid());
};
#endif

class ContentBrowserTestSuite : public ContentTestSuiteBase {
 public:
  ContentBrowserTestSuite(int argc, char** argv)
      : ContentTestSuiteBase(argc, argv) {
  }
  virtual ~ContentBrowserTestSuite() {
  }

 protected:
  virtual void Initialize() OVERRIDE {

#if defined(OS_ANDROID)
    base::i18n::AllowMultipleInitializeCallsForTesting();
    base::i18n::InitializeICU();

    // This needs to be done before base::TestSuite::Initialize() is called,
    // as it also tries to set MessagePumpForUIFactory.
    if (!base::MessageLoop::InitMessagePumpForUIFactory(
            &CreateMessagePumpForUI))
      VLOG(0) << "MessagePumpForUIFactory already set, unable to override.";

    // For all other platforms, we call ContentMain for browser tests which goes
    // through the normal browser initialization paths. For Android, we must set
    // things up manually.
    content_client_.reset(new ShellContentClient);
    browser_content_client_.reset(new ShellContentBrowserClient());
    SetContentClient(content_client_.get());
    SetBrowserClientForTesting(browser_content_client_.get());

    content::RegisterContentSchemes(false);
    RegisterPathProvider();
    ui::RegisterPathProvider();
    RegisterInProcessThreads();

    InitializeMojo();
#endif

    ContentTestSuiteBase::Initialize();
  }

#if defined(OS_ANDROID)
  scoped_ptr<ShellContentClient> content_client_;
  scoped_ptr<ShellContentBrowserClient> browser_content_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ContentBrowserTestSuite);
};

class ContentTestLauncherDelegate : public TestLauncherDelegate {
 public:
  ContentTestLauncherDelegate() {}
  virtual ~ContentTestLauncherDelegate() {}

  virtual int RunTestSuite(int argc, char** argv) OVERRIDE {
    return ContentBrowserTestSuite(argc, argv).Run();
  }

  virtual bool AdjustChildProcessCommandLine(
      CommandLine* command_line, const base::FilePath& temp_data_dir) OVERRIDE {
    command_line->AppendSwitchPath(switches::kContentShellDataPath,
                                   temp_data_dir);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    return true;
  }

 protected:
  virtual ContentMainDelegate* CreateContentMainDelegate() OVERRIDE {
    return new ShellMainDelegate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentTestLauncherDelegate);
};

}  // namespace content

int main(int argc, char** argv) {
  int default_jobs = std::max(1, base::SysInfo::NumberOfProcessors() / 2);
  content::ContentTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, default_jobs, argc, argv);
}
