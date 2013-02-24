// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test_utils.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/shell/shell_javascript_dialog_manager.h"
#include "net/base/net_util.h"

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
  NavigationController* controller = &window->web_contents()->GetController();
  TestNavigationObserver same_tab_observer(
      Source<NavigationController>(controller),
      NULL,
      number_of_navigations);

  window->LoadURL(url);

  base::RunLoop run_loop;
  same_tab_observer.WaitForObservation(
      base::Bind(&RunThisRunLoop, base::Unretained(&run_loop)),
      GetQuitTaskForRunLoop(&run_loop));
}

void NavigateToURL(Shell* window, const GURL& url) {
  NavigateToURLBlockUntilNavigationsComplete(window, url, 1);
}

void WaitForAppModalDialog(Shell* window) {
  ShellJavaScriptDialogManager* dialog_manager=
      static_cast<ShellJavaScriptDialogManager*>(
          window->GetJavaScriptDialogManager());

  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
  dialog_manager->set_dialog_request_callback(runner->QuitClosure());
  runner->Run();
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
  if (runner_)
    runner_->QuitClosure().Run();
}

}  // namespace content
