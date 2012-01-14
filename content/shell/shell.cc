// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell.h"

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "content/browser/tab_contents/navigation_controller_impl.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/gfx/size.h"

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

Shell* Shell::CreateShell(TabContents* tab_contents) {
  Shell* shell = new Shell();
  shell->PlatformCreateWindow(kTestWindowWidth, kTestWindowHeight);

  shell->tab_contents_.reset(tab_contents);
  tab_contents->SetDelegate(shell);

  shell->PlatformSetContents();

  shell->PlatformResizeSubViews();
  return shell;
}

Shell* Shell::CreateNewWindow(content::BrowserContext* browser_context,
                              const GURL& url,
                              SiteInstance* site_instance,
                              int routing_id,
                              TabContents* base_tab_contents) {
  TabContents* tab_contents = new TabContents(
      browser_context,
      site_instance,
      routing_id,
      base_tab_contents,
      NULL);
  Shell* shell = CreateShell(tab_contents);
  if (!url.is_empty())
    shell->LoadURL(url);
  return shell;
}

void Shell::LoadURL(const GURL& url) {
  tab_contents_->GetController().LoadURL(
      url,
      content::Referrer(),
      content::PAGE_TRANSITION_TYPED,
      std::string());
  tab_contents_->Focus();
}

void Shell::GoBackOrForward(int offset) {
  tab_contents_->GetController().GoToOffset(offset);
  tab_contents_->Focus();
}

void Shell::Reload() {
  tab_contents_->GetController().Reload(false);
  tab_contents_->Focus();
}

void Shell::Stop() {
  tab_contents_->Stop();
  tab_contents_->Focus();
}

void Shell::UpdateNavigationControls() {
  int current_index = tab_contents_->GetController().GetCurrentEntryIndex();
  int max_index = tab_contents_->GetController().GetEntryCount() - 1;

  PlatformEnableUIControl(BACK_BUTTON, current_index > 0);
  PlatformEnableUIControl(FORWARD_BUTTON, current_index < max_index);
  PlatformEnableUIControl(STOP_BUTTON, tab_contents_->IsLoading());
}

gfx::NativeView Shell::GetContentView() {
  if (!tab_contents_.get())
    return NULL;
  return tab_contents_->GetNativeView();
}

void Shell::LoadingStateChanged(WebContents* source) {
  UpdateNavigationControls();
}

void Shell::WebContentsCreated(WebContents* source_contents,
                               int64 source_frame_id,
                               const GURL& target_url,
                               WebContents* new_contents) {
  CreateShell(static_cast<TabContents*>(new_contents));
}

void Shell::DidNavigateMainFramePostCommit(WebContents* tab) {
  PlatformSetAddressBarURL(tab->GetURL());
}

void Shell::UpdatePreferredSize(WebContents* source,
                                const gfx::Size& pref_size) {
  PlatformSizeTo(pref_size.width(), pref_size.height());
}

}  // namespace content
