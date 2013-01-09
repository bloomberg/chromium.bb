// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/render_view_context_menu_win.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/web_contents.h"
#include "win8/util/win8_util.h"

using content::WebContents;

RenderViewContextMenuWin::RenderViewContextMenuWin(
    WebContents* web_contents,
    const content::ContextMenuParams& params)
    : RenderViewContextMenuViews(web_contents, params) {
}

RenderViewContextMenuWin::~RenderViewContextMenuWin() {
}

// static
RenderViewContextMenuViews* RenderViewContextMenuViews::Create(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  return new RenderViewContextMenuWin(web_contents, params);
}

bool RenderViewContextMenuWin::IsCommandIdVisible(int command_id) const {
  // In windows 8 metro mode no new window option on normal browser windows.
  if (win8::IsSingleWindowMetroMode() && !profile_->IsOffTheRecord() &&
      command_id == IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW) {
    return false;
  }
  return RenderViewContextMenu::IsCommandIdVisible(command_id);
}

void RenderViewContextMenuWin::ExecuteCommand(int command_id) {
  ExecuteCommand(command_id, 0);
}

void RenderViewContextMenuWin::ExecuteCommand(int command_id,
                                              int event_flags) {
  if (win8::IsSingleWindowMetroMode() &&
      command_id == IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW) {
    // The open link in new window command should only be enabled for
    // incognito windows in metro mode.
    DCHECK(profile_->IsOffTheRecord());
    // We directly go to the Browser object to open the url in effect
    // bypassing the delegate. Currently the Browser is the only class which
    // implements the delegate for the context menu. This would break if there
    // are other delegates for the context menu. This is ok for now as this
    // code only executes for Windows 8 metro mode.
    // TODO(robertshield): FTB - Switch this to HOST_DESKTOP_TYPE_ASH when
    //                     we make that the default for metro.
    Browser* browser =
        chrome::FindTabbedBrowser(profile_->GetOriginalProfile(),
                                  false,
                                  chrome::HOST_DESKTOP_TYPE_NATIVE);
    if (browser) {
      content::OpenURLParams url_params(
          params_.link_url,
          content::Referrer(params_.frame_url.is_empty() ?
                                params_.page_url : params_.frame_url,
                            params_.referrer_policy),
          NEW_FOREGROUND_TAB,
          content::PAGE_TRANSITION_LINK,
          false);
      WebContents* source_web_contents = chrome::GetActiveWebContents(browser);
      WebContents* new_contents = source_web_contents->OpenURL(url_params);
      DCHECK(new_contents);
      return;
    }
  }
  RenderViewContextMenu::ExecuteCommand(command_id, event_flags);
}

void RenderViewContextMenuWin::SetExternal() {
  external_ = true;
}
