// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell.h"

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/shell/shell_browser_context.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include "content/browser/tab_contents/tab_contents_view_win.h"
#endif

// Content area size for newly created windows.
static const int kTestWindowWidth = 800;
static const int kTestWindowHeight = 600;

namespace content {

int Shell::shell_count_;

Shell::Shell()
    : main_window_(NULL),
      edit_window_(NULL)
#if defined(OS_WIN)
      , default_edit_wnd_proc_(0)
#endif
  {
    shell_count_++;
}

Shell::~Shell() {
  PlatformCleanUp();
  --shell_count_;
}

Shell* Shell::CreateNewWindow(ShellBrowserContext* browser_context) {
  Shell* shell = new Shell();
  shell->PlatformCreateWindow();

  shell->PlatformSizeTo(kTestWindowWidth, kTestWindowHeight);

  shell->tab_contents_.reset(new TabContents(
      browser_context,
      NULL,
      MSG_ROUTING_NONE,
      NULL,
      NULL));

#if defined (OS_WIN)
  TabContentsViewWin* view =
      static_cast<TabContentsViewWin*>(shell->tab_contents_->view());
  view->SetParent(shell->main_window_);
#endif

  shell->PlatformResizeSubViews();

  shell->LoadURL(GURL("http://www.google.com"));
  return shell;
}

void Shell::LoadURL(const GURL& url) {
  tab_contents_->controller().LoadURL(
      url,
      GURL(),
      PageTransition::TYPED,
      std::string());
}

void Shell::GoBackOrForward(int offset) {
  tab_contents_->controller().GoToOffset(offset);
}

void Shell::Reload() {
  tab_contents_->controller().Reload(false);
}

void Shell::Stop() {
  tab_contents_->Stop();
}

void Shell::UpdateNavigationControls() {
  int current_index = tab_contents_->controller().GetCurrentEntryIndex();
  int max_index = tab_contents_->controller().entry_count() - 1;

  PlatformEnableUIControl(BACK_BUTTON, current_index > 0);
  PlatformEnableUIControl(FORWARD_BUTTON, current_index < max_index);
  PlatformEnableUIControl(STOP_BUTTON, true);//is_loading_);
}

gfx::NativeView Shell::GetContentView() {
  if (!tab_contents_.get())
    return NULL;
  return tab_contents_->GetNativeView();
}

}  // namespace content
