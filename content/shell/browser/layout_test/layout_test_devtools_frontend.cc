// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_devtools_frontend.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/webkit_test_controller.h"
#include "net/base/filename_util.h"

namespace content {

// static
LayoutTestDevToolsFrontend* LayoutTestDevToolsFrontend::Show(
    WebContents* inspected_contents,
    const std::string& settings,
    const std::string& frontend_url) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_contents));
  Shell* shell = Shell::CreateNewWindow(inspected_contents->GetBrowserContext(),
                                        GURL(),
                                        NULL,
                                        gfx::Size());
  LayoutTestDevToolsFrontend* devtools_frontend =
      new LayoutTestDevToolsFrontend(shell, agent.get());

  shell->LoadURL(GetDevToolsPathAsURL(settings, frontend_url));

  return devtools_frontend;
}

// static.
GURL LayoutTestDevToolsFrontend::GetDevToolsPathAsURL(
    const std::string& settings,
    const std::string& frontend_url) {
  if (!frontend_url.empty())
    return GURL(frontend_url);
  base::FilePath dir_exe;
  if (!PathService::Get(base::DIR_EXE, &dir_exe)) {
    NOTREACHED();
    return GURL();
  }
#if defined(OS_MACOSX)
  // On Mac, the executable is in
  // out/Release/Content Shell.app/Contents/MacOS/Content Shell.
  // We need to go up 3 directories to get to out/Release.
  dir_exe = dir_exe.AppendASCII("../../..");
#endif
  base::FilePath dev_tools_path =
      dir_exe.AppendASCII("resources/inspector/devtools.html");

  GURL result = net::FilePathToFileURL(dev_tools_path);
  if (!settings.empty())
    result = GURL(base::StringPrintf("%s?settings=%s&experiments=true",
                                     result.spec().c_str(),
                                     settings.c_str()));
  return result;
}

void LayoutTestDevToolsFrontend::ReuseFrontend(WebContents* inspected_contents,
                                               const std::string& settings,
                                               const std::string frontend_url) {
  AttachTo(inspected_contents);
  frontend_shell()->LoadURL(GetDevToolsPathAsURL(settings, frontend_url));
}

LayoutTestDevToolsFrontend::LayoutTestDevToolsFrontend(
    Shell* frontend_shell,
    DevToolsAgentHost* agent_host)
    : ShellDevToolsFrontend(frontend_shell, agent_host) {
}

LayoutTestDevToolsFrontend::~LayoutTestDevToolsFrontend() {
}

void LayoutTestDevToolsFrontend::AgentHostClosed(
    DevToolsAgentHost* agent_host, bool replaced) {
  // Do not close the front-end shell.
}

void LayoutTestDevToolsFrontend::RenderProcessGone(
    base::TerminationStatus status) {
  WebKitTestController::Get()->DevToolsProcessCrashed();
}

}  // namespace content
