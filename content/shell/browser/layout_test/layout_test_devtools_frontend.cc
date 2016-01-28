// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_devtools_frontend.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/blink_test_controller.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"

namespace content {

// static
LayoutTestDevToolsFrontend* LayoutTestDevToolsFrontend::Show(
    WebContents* inspected_contents,
    const std::string& settings,
    const std::string& frontend_url) {
  Shell* shell = Shell::CreateNewWindow(inspected_contents->GetBrowserContext(),
                                        GURL(),
                                        NULL,
                                        gfx::Size());
  LayoutTestDevToolsFrontend* devtools_frontend =
      new LayoutTestDevToolsFrontend(shell, inspected_contents);

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
      dir_exe.AppendASCII("resources/inspector/inspector.html");

  GURL result = net::FilePathToFileURL(dev_tools_path);
  std::string url_string =
      base::StringPrintf("%s?experiments=true", result.spec().c_str());
#if defined(DEBUG_DEVTOOLS)
  url_string += "&debugFrontend=true";
#endif  // defined(DEBUG_DEVTOOLS)
  if (!settings.empty())
    url_string += "&settings=" + settings;
  return GURL(url_string);
}

void LayoutTestDevToolsFrontend::ReuseFrontend(const std::string& settings,
                                               const std::string frontend_url) {
  DisconnectFromTarget();
  preferences()->Clear();
  frontend_shell()->LoadURL(GetDevToolsPathAsURL(settings, frontend_url));
}

LayoutTestDevToolsFrontend::LayoutTestDevToolsFrontend(
    Shell* frontend_shell,
    WebContents* inspected_contents)
    : ShellDevToolsFrontend(frontend_shell, inspected_contents) {
}

LayoutTestDevToolsFrontend::~LayoutTestDevToolsFrontend() {
}

void LayoutTestDevToolsFrontend::AgentHostClosed(
    DevToolsAgentHost* agent_host, bool replaced) {
  // Do not close the front-end shell.
}

void LayoutTestDevToolsFrontend::RenderProcessGone(
    base::TerminationStatus status) {
  BlinkTestController::Get()->DevToolsProcessCrashed();
}

}  // namespace content
