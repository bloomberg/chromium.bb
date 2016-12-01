// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_devtools_frontend.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/layout_test/blink_test_controller.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
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
  devtools_frontend->SetPreferences(settings);
  shell->LoadURL(GetDevToolsPathAsURL(frontend_url));
  return devtools_frontend;
}

// static.
GURL LayoutTestDevToolsFrontend::GetDevToolsPathAsURL(
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
  base::FilePath dev_tools_path;
  bool is_debug_dev_tools = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDebugDevTools);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCustomDevToolsFrontend)) {
    dev_tools_path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        switches::kCustomDevToolsFrontend);
  } else {
    std::string folder = is_debug_dev_tools ? "debug/" : "";
    dev_tools_path = dir_exe.AppendASCII("resources/inspector/" + folder);
  }

  GURL result =
      net::FilePathToFileURL(dev_tools_path.AppendASCII("inspector.html"));
  std::string url_string =
      base::StringPrintf("%s?experiments=true", result.spec().c_str());
  if (is_debug_dev_tools)
    url_string += "&debugFrontend=true";
  return GURL(url_string);
}

// static.
GURL LayoutTestDevToolsFrontend::MapJSTestURL(const GURL& test_url) {
  std::string url_string = GetDevToolsPathAsURL(std::string()).spec();
  std::string inspector_file_name = "inspector.html";
  size_t start_position = url_string.find(inspector_file_name);
  url_string.replace(start_position, inspector_file_name.length(),
                     "unit_test_runner.html");
  url_string += "&test=" + test_url.spec();
  return GURL(url_string);
}

void LayoutTestDevToolsFrontend::ReuseFrontend(const std::string& settings,
                                               const std::string frontend_url) {
  DisconnectFromTarget();
  SetPreferences(settings);
  ready_for_test_ = false;
  pending_evaluations_.clear();
  frontend_shell()->LoadURL(GetDevToolsPathAsURL(frontend_url));
}

void LayoutTestDevToolsFrontend::EvaluateInFrontend(
    int call_id,
    const std::string& script) {
  if (!ready_for_test_) {
    pending_evaluations_.push_back(std::make_pair(call_id, script));
    return;
  }

  std::string encoded_script;
  base::JSONWriter::Write(base::StringValue(script), &encoded_script);
  std::string source =
      base::StringPrintf("DevToolsAPI.evaluateForTestInFrontend(%d, %s);",
                         call_id,
                         encoded_script.c_str());
  web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(source));
}

LayoutTestDevToolsFrontend::LayoutTestDevToolsFrontend(
    Shell* frontend_shell,
    WebContents* inspected_contents)
    : ShellDevToolsFrontend(frontend_shell, inspected_contents),
      ready_for_test_(false) {
}

LayoutTestDevToolsFrontend::~LayoutTestDevToolsFrontend() {
}

void LayoutTestDevToolsFrontend::AgentHostClosed(
    DevToolsAgentHost* agent_host, bool replaced) {
  // Do not close the front-end shell.
}

void LayoutTestDevToolsFrontend::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  std::string method;
  base::DictionaryValue* dict = nullptr;
  std::unique_ptr<base::Value> parsed_message = base::JSONReader::Read(message);
  if (parsed_message &&
      parsed_message->GetAsDictionary(&dict) &&
      dict->GetString("method", &method) &&
      method == "readyForTest") {
    ready_for_test_ = true;
    for (const auto& pair : pending_evaluations_)
      EvaluateInFrontend(pair.first, pair.second);
    pending_evaluations_.clear();
    return;
  }

  ShellDevToolsFrontend::HandleMessageFromDevToolsFrontend(message);
}

void LayoutTestDevToolsFrontend::RenderProcessGone(
    base::TerminationStatus status) {
  BlinkTestController::Get()->DevToolsProcessCrashed();
}

void LayoutTestDevToolsFrontend::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  BlinkTestController::Get()->HandleNewRenderFrameHost(render_frame_host);
}

}  // namespace content
