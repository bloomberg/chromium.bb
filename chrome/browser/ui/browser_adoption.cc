// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

const char kWebDialogDelegateUserDataKey[] = "BrowserAdoptedAsTabContents";

}  // namespace

// static
void Browser::AdoptAsTabContents(WebContents* web_contents) {
  // If already adopted, nothing to be done.
  base::SupportsUserData::Data* adoption_tag =
      web_contents->GetUserData(&kWebDialogDelegateUserDataKey);
  if (adoption_tag)
    return;

  // Mark as adopted.
  web_contents->SetUserData(&kWebDialogDelegateUserDataKey,
                            new base::SupportsUserData::Data());

  // Create all the tab helpers.
  TabContents::Factory::CreateTabContents(web_contents);
  // ... more tab helper creation goes here ...
}

void Browser::SetAsDelegate(WebContents* web_contents, Browser* delegate) {
  // WebContents...
  web_contents->SetDelegate(delegate);

  // ...and all the helpers.
  TabContents* tab = TabContents::FromWebContents(web_contents);
  tab->blocked_content_tab_helper()->set_delegate(delegate);
  BookmarkTabHelper::FromWebContents(web_contents)->set_delegate(delegate);
  tab->constrained_window_tab_helper()->set_delegate(delegate);
  tab->core_tab_helper()->set_delegate(delegate);
  tab->search_engine_tab_helper()->set_delegate(delegate);
  tab->zoom_controller()->set_observer(delegate);
}
