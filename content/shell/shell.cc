// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell.h"

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include "content/browser/tab_contents/tab_contents_view_win.h"
#endif

// Content area size for newly created windows.
static const int kTestWindowWidth = 800;
static const int kTestWindowHeight = 600;

namespace content {

std::vector<Shell*> Shell::windows_;

Shell::Shell()
    : window_(NULL),
      url_edit_view_(NULL)
#if defined(OS_WIN)
      , default_edit_wnd_proc_(0)
#endif
  {
    windows_.push_back(this);
}

Shell::~Shell() {
  PlatformCleanUp();

  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i] == this) {
      windows_.erase(windows_.begin() + i);
      break;
    }
  }
}

Shell* Shell::CreateNewWindow(content::BrowserContext* browser_context,
                              const GURL& url,
                              SiteInstance* site_instance,
                              int routing_id,
                              TabContents* base_tab_contents) {
  Shell* shell = new Shell();
  shell->PlatformCreateWindow(kTestWindowWidth, kTestWindowHeight);

  shell->tab_contents_.reset(new TabContents(
      browser_context,
      site_instance,
      routing_id,
      base_tab_contents,
      NULL));
  shell->tab_contents_->set_delegate(shell);

#if defined(OS_WIN)
  TabContentsViewWin* view =
      static_cast<TabContentsViewWin*>(shell->tab_contents_->view());
  view->SetParent(shell->window_);
#endif

  shell->PlatformResizeSubViews();

  if (!url.is_empty())
    shell->LoadURL(url);
  return shell;
}

void Shell::LoadURL(const GURL& url) {
  tab_contents_->controller().LoadURL(
      url,
      content::Referrer(),
      content::PAGE_TRANSITION_TYPED,
      std::string());
  tab_contents_->Focus();
}

void Shell::GoBackOrForward(int offset) {
  tab_contents_->controller().GoToOffset(offset);
  tab_contents_->Focus();
}

void Shell::Reload() {
  tab_contents_->controller().Reload(false);
  tab_contents_->Focus();
}

void Shell::Stop() {
  tab_contents_->Stop();
  tab_contents_->Focus();
}

void Shell::UpdateNavigationControls() {
  int current_index = tab_contents_->controller().GetCurrentEntryIndex();
  int max_index = tab_contents_->controller().entry_count() - 1;

  PlatformEnableUIControl(BACK_BUTTON, current_index > 0);
  PlatformEnableUIControl(FORWARD_BUTTON, current_index < max_index);
  PlatformEnableUIControl(STOP_BUTTON, tab_contents_->IsLoading());
}

gfx::NativeView Shell::GetContentView() {
  if (!tab_contents_.get())
    return NULL;
  return tab_contents_->GetNativeView();
}

void Shell::LoadingStateChanged(TabContents* source) {
  UpdateNavigationControls();
}

void Shell::DidNavigateMainFramePostCommit(TabContents* tab) {
  PlatformSetAddressBarURL(tab->GetURL());
}

void Shell::UpdatePreferredSize(TabContents* source,
                                const gfx::Size& pref_size) {
  PlatformSizeTo(pref_size.width(), pref_size.height());
}

}  // namespace content
