// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_devtools_bindings.h"

#include <memory>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/layout_test/blink_test_controller.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "net/base/filename_util.h"

namespace {

GURL GetInspectedPageURL(const GURL& test_url) {
  std::string spec = test_url.spec();
  std::string test_query_param = "&test=";
  std::string test_script_url =
      spec.substr(spec.find(test_query_param) + test_query_param.length());
  std::string inspected_page_url = test_script_url.replace(
      test_script_url.find("/devtools/"), std::string::npos,
      "/devtools/resources/inspected-page.html");
  return GURL(inspected_page_url);
}

}  // namespace

namespace content {

class LayoutTestDevToolsBindings::SecondaryObserver
    : public WebContentsObserver {
 public:
  explicit SecondaryObserver(LayoutTestDevToolsBindings* bindings)
      : WebContentsObserver(bindings->inspected_contents()),
        bindings_(bindings) {}

  // WebContentsObserver implementation.
  void DocumentAvailableInMainFrame() override {
    if (bindings_)
      bindings_->NavigateDevToolsFrontend();
    bindings_ = nullptr;
  }

  // WebContentsObserver implementation.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    BlinkTestController::Get()->HandleNewRenderFrameHost(render_frame_host);
  }

 private:
  LayoutTestDevToolsBindings* bindings_;
  DISALLOW_COPY_AND_ASSIGN(SecondaryObserver);
};

// static.
GURL LayoutTestDevToolsBindings::GetDevToolsPathAsURL(
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

  GURL result = net::FilePathToFileURL(
      dev_tools_path.AppendASCII("integration_test_runner.html"));
  std::string url_string =
      base::StringPrintf("%s?experiments=true", result.spec().c_str());
  if (is_debug_dev_tools)
    url_string += "&debugFrontend=true";
  return GURL(url_string);
}

// static.
GURL LayoutTestDevToolsBindings::MapTestURLIfNeeded(const GURL& test_url,
                                                    bool* is_devtools_js_test) {
  std::string spec = test_url.spec();
  bool is_js_test =
      base::EndsWith(spec, ".js", base::CompareCase::INSENSITIVE_ASCII);
  *is_devtools_js_test =
      spec.find("/devtools/") != std::string::npos && is_js_test;
  if (!*is_devtools_js_test)
    return test_url;
  std::string url_string = GetDevToolsPathAsURL(std::string()).spec();
  url_string += "&test=" + spec;
  return GURL(url_string);
}

void LayoutTestDevToolsBindings::NavigateDevToolsFrontend() {
  GURL devtools_url = GetDevToolsPathAsURL(frontend_url_.spec());
  NavigationController::LoadURLParams params(devtools_url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents()->GetController().LoadURLWithParams(params);
  web_contents()->Focus();
}

void LayoutTestDevToolsBindings::EvaluateInFrontend(int call_id,
                                                    const std::string& script) {
  if (!ready_for_test_) {
    pending_evaluations_.push_back(std::make_pair(call_id, script));
    return;
  }

  std::string encoded_script;
  base::JSONWriter::Write(base::Value(script), &encoded_script);
  std::string source =
      base::StringPrintf("DevToolsAPI.evaluateForTestInFrontend(%d, %s);",
                         call_id, encoded_script.c_str());
  web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(source));
}

LayoutTestDevToolsBindings::LayoutTestDevToolsBindings(
    WebContents* devtools_contents,
    WebContents* inspected_contents,
    const std::string& settings,
    const GURL& frontend_url,
    bool new_harness)
    : ShellDevToolsBindings(devtools_contents, inspected_contents, nullptr),
      frontend_url_(frontend_url) {
  SetPreferences(settings);

  if (new_harness) {
    secondary_observer_ = std::make_unique<SecondaryObserver>(this);
    NavigationController::LoadURLParams params(
        GetInspectedPageURL(frontend_url));
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    inspected_contents->GetController().LoadURLWithParams(params);
  } else {
    NavigateDevToolsFrontend();
  }
}

LayoutTestDevToolsBindings::~LayoutTestDevToolsBindings() {}

void LayoutTestDevToolsBindings::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  std::string method;
  base::DictionaryValue* dict = nullptr;
  std::unique_ptr<base::Value> parsed_message = base::JSONReader::Read(message);
  if (parsed_message && parsed_message->GetAsDictionary(&dict) &&
      dict->GetString("method", &method) && method == "readyForTest") {
    ready_for_test_ = true;
    for (const auto& pair : pending_evaluations_)
      EvaluateInFrontend(pair.first, pair.second);
    pending_evaluations_.clear();
    return;
  }

  ShellDevToolsBindings::HandleMessageFromDevToolsFrontend(message);
}

void LayoutTestDevToolsBindings::RenderProcessGone(
    base::TerminationStatus status) {
  BlinkTestController::Get()->DevToolsProcessCrashed();
}

void LayoutTestDevToolsBindings::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  BlinkTestController::Get()->HandleNewRenderFrameHost(render_frame_host);
}

}  // namespace content
