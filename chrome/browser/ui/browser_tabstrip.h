// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TABSTRIP_H_
#define CHROME_BROWSER_UI_BROWSER_TABSTRIP_H_
#pragma once

#include "content/public/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class GURL;
class Profile;
class TabContents;

namespace content {
class SessionStorageNamespace;
class SiteInstance;
class WebContents;
}

namespace gfx {
class Rect;
}

namespace chrome {

int GetIndexOfTab(const Browser* browser, const content::WebContents* contents);

TabContents* GetActiveTabContents(const Browser* browser);
content::WebContents* GetActiveWebContents(const Browser* browser);

TabContents* GetTabContentsAt(const Browser* browser, int index);
content::WebContents* GetWebContentsAt(const Browser* browser, int index);

void ActivateTabAt(Browser* browser, int index, bool user_gesture);

// Used by extensions.
bool IsTabStripEditable(Browser* browser);

// Adds a selected tab with the specified URL and transition, returns the
// created TabContents.
TabContents* AddSelectedTabWithURL(Browser* browser,
                                   const GURL& url,
                                   content::PageTransition transition);

// Adds |tab_contents| to |browser|'s tabstrip.
void AddTab(Browser* browser,
            TabContents* tab_contents,
            content::PageTransition type);

// Creates a new tab with the already-created WebContents 'new_contents'.
// The window for the added contents will be reparented correctly when this
// method returns.  If |disposition| is NEW_POPUP, |pos| should hold the
// initial position.
void AddWebContents(Browser* browser,
                    content::WebContents* source_contents,
                    content::WebContents* new_contents,
                    WindowOpenDisposition disposition,
                    const gfx::Rect& initial_pos,
                    bool user_gesture);
void CloseWebContents(Browser* browser, content::WebContents* contents);

void CloseAllTabs(Browser* browser);

// Centralized method for creating a TabContents, configuring and
// installing all its supporting objects and observers.
TabContents* TabContentsFactory(
    Profile* profile,
    content::SiteInstance* site_instance,
    int routing_id,
    const content::WebContents* base_web_contents,
    content::SessionStorageNamespace* session_storage_namespace);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TABSTRIP_H_
