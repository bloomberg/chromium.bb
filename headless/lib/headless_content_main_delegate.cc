// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/headless_content_main_delegate.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_content_browser_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace headless {
namespace {
// Keep in sync with content/common/content_constants_internal.h.
// TODO(skyostil): Add a tracing test for this.
const int kTraceEventBrowserProcessSortIndex = -6;

HeadlessContentMainDelegate* g_current_headless_content_main_delegate = nullptr;
}  // namespace

HeadlessContentMainDelegate::HeadlessContentMainDelegate(
    std::unique_ptr<HeadlessBrowserImpl> browser)
    : content_client_(browser->options()), browser_(std::move(browser)) {
  DCHECK(!g_current_headless_content_main_delegate);
  g_current_headless_content_main_delegate = this;
}

HeadlessContentMainDelegate::~HeadlessContentMainDelegate() {
  DCHECK(g_current_headless_content_main_delegate == this);
  g_current_headless_content_main_delegate = nullptr;
}

bool HeadlessContentMainDelegate::BasicStartupComplete(int* exit_code) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Make sure all processes know that we're in headless mode.
  if (!command_line->HasSwitch(switches::kHeadless))
    command_line->AppendSwitch(switches::kHeadless);

  if (browser_->options()->single_process_mode)
    command_line->AppendSwitch(switches::kSingleProcess);

  if (browser_->options()->disable_sandbox)
    command_line->AppendSwitch(switches::kNoSandbox);

#if defined(USE_OZONE)
  // The headless backend is automatically chosen for a headless build, but also
  // adding it here allows us to run in a non-headless build too.
  command_line->AppendSwitchASCII(switches::kOzonePlatform, "headless");
#endif

  if (!browser_->options()->gl_implementation.empty()) {
    command_line->AppendSwitchASCII(switches::kUseGL,
                                    browser_->options()->gl_implementation);
  } else {
    command_line->AppendSwitch(switches::kDisableGpu);
  }

  SetContentClient(&content_client_);
  return false;
}

void HeadlessContentMainDelegate::PreSandboxStartup() {
  InitializeResourceBundle();
}

int HeadlessContentMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (!process_type.empty())
    return -1;

  base::trace_event::TraceLog::GetInstance()->SetProcessName("HeadlessBrowser");
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventBrowserProcessSortIndex);

  std::unique_ptr<content::BrowserMainRunner> browser_runner(
      content::BrowserMainRunner::Create());

  int exit_code = browser_runner->Initialize(main_function_params);
  DCHECK_LT(exit_code, 0) << "content::BrowserMainRunner::Initialize failed in "
                             "HeadlessContentMainDelegate::RunProcess";

  browser_->RunOnStartCallback();
  browser_runner->Run();
  browser_.reset();
  browser_runner->Shutdown();

  // Return value >=0 here to disable calling content::BrowserMain.
  return 0;
}

void HeadlessContentMainDelegate::ZygoteForked() {
  // TODO(skyostil): Disable the zygote host.
}

// static
HeadlessContentMainDelegate* HeadlessContentMainDelegate::GetInstance() {
  return g_current_headless_content_main_delegate;
}

// static
void HeadlessContentMainDelegate::InitializeResourceBundle() {
  base::FilePath dir_module;
  base::FilePath pak_file;
  bool result = PathService::Get(base::DIR_MODULE, &dir_module);
  DCHECK(result);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string locale = command_line->GetSwitchValueASCII(switches::kLang);
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      locale, nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

  // Try loading the headless library pak file first. If it doesn't exist (i.e.,
  // when we're running with the --headless switch), fall back to the browser's
  // resource pak.
  pak_file = dir_module.Append(FILE_PATH_LITERAL("headless_lib.pak"));
  if (!base::PathExists(pak_file))
    pak_file = dir_module.Append(FILE_PATH_LITERAL("resources.pak"));
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_NONE);
}

content::ContentBrowserClient*
HeadlessContentMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(new HeadlessContentBrowserClient(browser_.get()));
  return browser_client_.get();
}

}  // namespace headless
