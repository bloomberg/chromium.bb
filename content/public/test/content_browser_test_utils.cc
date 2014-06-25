// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_utils.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
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

void LoadDataWithBaseURL(Shell* window, const GURL& url,
    const std::string data, const GURL& base_url) {
  WaitForLoadStop(window->web_contents());
  TestNavigationObserver same_tab_observer(window->web_contents(), 1);

  window->LoadDataWithBaseURL(url, data, base_url);
  same_tab_observer.Wait();
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
  if (runner_.get())
    runner_->QuitClosure().Run();
}

class RenderViewCreatedObserver : public WebContentsObserver {
 public:
  explicit RenderViewCreatedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        render_view_created_called_(false) {
  }

  // WebContentsObserver:
  virtual void RenderViewCreated(RenderViewHost* rvh) OVERRIDE {
    render_view_created_called_ = true;
  }

  bool render_view_created_called_;
};

WebContentsAddedObserver::WebContentsAddedObserver()
    : web_contents_created_callback_(
          base::Bind(
              &WebContentsAddedObserver::WebContentsCreated,
              base::Unretained(this))),
      web_contents_(NULL) {
  WebContentsImpl::AddCreatedCallback(web_contents_created_callback_);
}

WebContentsAddedObserver::~WebContentsAddedObserver() {
  WebContentsImpl::RemoveCreatedCallback(web_contents_created_callback_);
}

void WebContentsAddedObserver::WebContentsCreated(WebContents* web_contents) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;
  child_observer_.reset(new RenderViewCreatedObserver(web_contents));

  if (runner_.get())
    runner_->QuitClosure().Run();
}

WebContents* WebContentsAddedObserver::GetWebContents() {
  if (web_contents_)
    return web_contents_;

  runner_ = new MessageLoopRunner();
  runner_->Run();
  return web_contents_;
}

bool WebContentsAddedObserver::RenderViewCreatedCalled() {
  if (child_observer_)
    return child_observer_->render_view_created_called_;

  return false;
}

}  // namespace content
