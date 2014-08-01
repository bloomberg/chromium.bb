// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_devtools_frontend.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_delegate.h"
#include "content/shell/browser/webkit_test_controller.h"
#include "content/shell/common/shell_switches.h"
#include "net/base/filename_util.h"

namespace content {

// DevTools frontend path for inspector LayoutTests.
GURL GetDevToolsPathAsURL(const std::string& settings,
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
  base::FilePath dev_tools_path = dir_exe.AppendASCII(
      "resources/inspector/devtools.html");

  GURL result = net::FilePathToFileURL(dev_tools_path);
  if (!settings.empty())
      result = GURL(base::StringPrintf("%s?settings=%s&experiments=true",
                                       result.spec().c_str(),
                                       settings.c_str()));
  return result;
}

// static
ShellDevToolsFrontend* ShellDevToolsFrontend::Show(
    WebContents* inspected_contents) {
  return ShellDevToolsFrontend::Show(inspected_contents, "", "");
}

// static
ShellDevToolsFrontend* ShellDevToolsFrontend::Show(
    WebContents* inspected_contents,
    const std::string& settings,
    const std::string& frontend_url) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(
          inspected_contents->GetRenderViewHost()));
  Shell* shell = Shell::CreateNewWindow(inspected_contents->GetBrowserContext(),
                                        GURL(),
                                        NULL,
                                        MSG_ROUTING_NONE,
                                        gfx::Size());
  ShellDevToolsFrontend* devtools_frontend = new ShellDevToolsFrontend(
      shell,
      agent.get());

  ShellDevToolsDelegate* delegate = ShellContentBrowserClient::Get()->
      shell_browser_main_parts()->devtools_delegate();
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    shell->LoadURL(GetDevToolsPathAsURL(settings, frontend_url));
  else
    shell->LoadURL(delegate->devtools_http_handler()->GetFrontendURL());

  return devtools_frontend;
}

void ShellDevToolsFrontend::Activate() {
  frontend_shell_->ActivateContents(web_contents());
}

void ShellDevToolsFrontend::Focus() {
  web_contents()->Focus();
}

void ShellDevToolsFrontend::InspectElementAt(int x, int y) {
  agent_host_->InspectElement(x, y);
}

void ShellDevToolsFrontend::Close() {
  frontend_shell_->Close();
}

ShellDevToolsFrontend::ShellDevToolsFrontend(Shell* frontend_shell,
                                             DevToolsAgentHost* agent_host)
    : WebContentsObserver(frontend_shell->web_contents()),
      frontend_shell_(frontend_shell),
      agent_host_(agent_host) {
}

ShellDevToolsFrontend::~ShellDevToolsFrontend() {
}

void ShellDevToolsFrontend::RenderViewCreated(
    RenderViewHost* render_view_host) {
  if (!frontend_host_) {
    frontend_host_.reset(DevToolsFrontendHost::Create(render_view_host, this));
    DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        agent_host_.get(), this);
  }
}

void ShellDevToolsFrontend::DocumentOnLoadCompletedInMainFrame() {
  web_contents()->GetMainFrame()->ExecuteJavaScript(
      base::ASCIIToUTF16("InspectorFrontendAPI.setUseSoftMenu(true);"));
}

void ShellDevToolsFrontend::WebContentsDestroyed() {
  DevToolsManager::GetInstance()->ClientHostClosing(this);
  delete this;
}

void ShellDevToolsFrontend::RenderProcessGone(base::TerminationStatus status) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    WebKitTestController::Get()->DevToolsProcessCrashed();
}

void ShellDevToolsFrontend::HandleMessageFromDevToolsFrontendToBackend(
    const std::string& message) {
  DevToolsManager::GetInstance()->DispatchOnInspectorBackend(
      this, message);
}

void ShellDevToolsFrontend::DispatchOnInspectorFrontend(
    const std::string& message) {
  if (frontend_host_)
    frontend_host_->DispatchOnDevToolsFrontend(message);
}

void ShellDevToolsFrontend::InspectedContentsClosing() {
  frontend_shell_->Close();
}

}  // namespace content
