// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/content_main.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "content/app/content_main_delegate.h"
#include "content/common/content_switches.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/shell/shell_content_client.h"
#include "content/shell/shell_content_plugin_client.h"
#include "content/shell/shell_content_renderer_client.h"
#include "content/shell/shell_content_utility_client.h"
#include "sandbox/src/sandbox_types.h"

#if defined(OS_WIN)
#include "content/app/startup_helper_win.h"
#endif

namespace {

class ShellMainDelegate : public content::ContentMainDelegate {
 public:
  virtual void PreSandboxStartup() OVERRIDE {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);

    content::SetContentClient(&content_client_);
    InitializeShellContentClient(process_type);
  }

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual void ZygoteForked() OVERRIDE {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);
    InitializeShellContentClient(process_type);
  }
#endif

 private:
  void InitializeShellContentClient(const std::string& process_type) {
    if (process_type.empty()) {
      browser_client_.reset(new content::ShellContentBrowserClient);
      content::GetContentClient()->set_browser(browser_client_.get());
    } else if (process_type == switches::kRendererProcess) {
      renderer_client_.reset(new content::ShellContentRendererClient);
      content::GetContentClient()->set_renderer(renderer_client_.get());
    } else if (process_type == switches::kPluginProcess) {
      plugin_client_.reset(new content::ShellContentPluginClient);
      content::GetContentClient()->set_plugin(plugin_client_.get());
    } else if (process_type == switches::kUtilityProcess) {
      utility_client_.reset(new content::ShellContentUtilityClient);
      content::GetContentClient()->set_utility(utility_client_.get());
    }
  }

  scoped_ptr<content::ShellContentBrowserClient> browser_client_;
  scoped_ptr<content::ShellContentRendererClient> renderer_client_;
  scoped_ptr<content::ShellContentPluginClient> plugin_client_;
  scoped_ptr<content::ShellContentUtilityClient> utility_client_;
  content::ShellContentClient content_client_;
};

}  // namespace

#if defined(OS_WIN)

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);
  ShellMainDelegate delegate;
  return content::ContentMain(instance, &sandbox_info, &delegate);
}
#endif

#if defined(OS_POSIX)

#if defined(OS_MACOSX)
__attribute__((visibility("default")))
int main(int argc, const char* argv[]) {
#else
int main(int argc, const char** argv) {
#endif  // OS_MACOSX

  ShellMainDelegate delegate;
  return content::ContentMain(argc, argv, &delegate);
}

#endif  // OS_POSIX
