// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

// The android implementation does not do anything "foreign session" specific.
// We use it to restore tabs from "recently closed" too.
// static
void SessionRestore::RestoreForeignSessionTab(
    content::WebContents* web_contents,
    const SessionTab& session_tab,
    WindowOpenDisposition disposition) {
  DCHECK(session_tab.navigations.size() > 0);
  content::BrowserContext* context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(context);
  TabModel* tab_model = TabModelList::GetTabModelWithProfile(profile);
  DCHECK(tab_model);
  std::vector<content::NavigationEntry*> entries =
      TabNavigation::CreateNavigationEntriesFromTabNavigations(
          session_tab.navigations, profile);
  content::WebContents* new_web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(context));
  int selected_index = session_tab.normalized_navigation_index();
  new_web_contents->GetController().Restore(
      selected_index,
      content::NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY,
      &entries);
  tab_model->CreateTab(new_web_contents);
}

// static
void SessionRestore::RestoreForeignSessionWindows(
    Profile* profile,
    chrome::HostDesktopType host_desktop_type,
    std::vector<const SessionWindow*>::const_iterator begin,
    std::vector<const SessionWindow*>::const_iterator end) {
  NOTREACHED();
}
