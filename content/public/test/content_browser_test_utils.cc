// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_utils.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "net/base/filename_util.h"

namespace content {

base::FilePath GetTestFilePath(const char* dir, const char* file) {
  base::FilePath path;
  PathService::Get(DIR_TEST_DATA, &path);
  return path.Append(base::FilePath().AppendASCII(dir).Append(
      base::FilePath().AppendASCII(file)));
}

GURL GetTestUrl(const char* dir, const char* file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}

void NavigateToURLBlockUntilNavigationsComplete(Shell* window,
                                                const GURL& url,
                                                int number_of_navigations) {
  WaitForLoadStop(window->web_contents());
  TestNavigationObserver same_tab_observer(window->web_contents(),
                                           number_of_navigations);

  window->LoadURL(url);
  same_tab_observer.Wait();
}

void ReloadBlockUntilNavigationsComplete(Shell* window,
                                         int number_of_navigations) {
  WaitForLoadStop(window->web_contents());
  TestNavigationObserver same_tab_observer(window->web_contents(),
                                           number_of_navigations);

  window->Reload();
  same_tab_observer.Wait();
}

void ReloadBypassingCacheBlockUntilNavigationsComplete(
    Shell* window,
    int number_of_navigations) {
  WaitForLoadStop(window->web_contents());
  TestNavigationObserver same_tab_observer(window->web_contents(),
                                           number_of_navigations);

  window->ReloadBypassingCache();
  same_tab_observer.Wait();
}

void LoadDataWithBaseURL(Shell* window,
                         const GURL& url,
                         const std::string& data,
                         const GURL& base_url) {
  WaitForLoadStop(window->web_contents());
  TestNavigationObserver same_tab_observer(window->web_contents(), 1);

  window->LoadDataWithBaseURL(url, data, base_url);
  same_tab_observer.Wait();
}

bool NavigateToURL(Shell* window, const GURL& url) {
  NavigateToURLBlockUntilNavigationsComplete(window, url, 1);
  if (!IsLastCommittedEntryOfPageType(window->web_contents(),
                                      PAGE_TYPE_NORMAL))
    return false;
  return window->web_contents()->GetLastCommittedURL() == url;
}

bool NavigateToURLAndExpectNoCommit(Shell* window, const GURL& url) {
  NavigationEntry* old_entry =
      window->web_contents()->GetController().GetLastCommittedEntry();
  NavigateToURLBlockUntilNavigationsComplete(window, url, 1);
  NavigationEntry* new_entry =
      window->web_contents()->GetController().GetLastCommittedEntry();
  return old_entry == new_entry;
}

void WaitForAppModalDialog(Shell* window) {
  ShellJavaScriptDialogManager* dialog_manager =
      static_cast<ShellJavaScriptDialogManager*>(
          window->GetJavaScriptDialogManager(window->web_contents()));

  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
  dialog_manager->set_dialog_request_callback(runner->QuitClosure());
  runner->Run();
}

RenderFrameHost* ConvertToRenderFrameHost(Shell* shell) {
  return shell->web_contents()->GetMainFrame();
}

ShellAddedObserver::ShellAddedObserver()
    : shell_(NULL) {
  Shell::SetShellCreatedCallback(
      base::Bind(&ShellAddedObserver::ShellCreated, base::Unretained(this)));
}

ShellAddedObserver::~ShellAddedObserver() {
}

Shell* ShellAddedObserver::GetShell() {
  if (shell_)
    return shell_;

  runner_ = new MessageLoopRunner();
  runner_->Run();
  return shell_;
}

void ShellAddedObserver::ShellCreated(Shell* shell) {
  DCHECK(!shell_);
  shell_ = shell;
  if (runner_.get())
    runner_->QuitClosure().Run();
}

ConsoleObserverDelegate::ConsoleObserverDelegate(WebContents* web_contents,
                                                 const std::string& filter)
    : web_contents_(web_contents),
      filter_(filter),
      message_loop_runner_(new MessageLoopRunner) {}

ConsoleObserverDelegate::~ConsoleObserverDelegate() {}

void ConsoleObserverDelegate::Wait() {
  message_loop_runner_->Run();
}

bool ConsoleObserverDelegate::AddMessageToConsole(
    WebContents* source,
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  DCHECK(source == web_contents_);

  std::string ascii_message = base::UTF16ToASCII(message);
  if (base::MatchPattern(ascii_message, filter_)) {
    message_ = ascii_message;
    message_loop_runner_->Quit();
  }
  return false;
}

}  // namespace content
