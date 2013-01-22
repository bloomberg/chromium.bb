// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TABSTRIP_H_
#define CHROME_BROWSER_UI_BROWSER_TABSTRIP_H_

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class GURL;
class Profile;

namespace content {
class SiteInstance;
}

namespace gfx {
class Rect;
}

namespace chrome {

content::WebContents* GetActiveWebContents(const Browser* browser);

// Adds a blank tab to the tab strip of the specified browser; an |index| of -1
// means to append it to the end of the tab strip.
void AddBlankTabAt(Browser* browser, int index, bool foreground);

// Adds a selected tab with the specified URL and transition, returns the
// created WebContents.
content::WebContents* AddSelectedTabWithURL(Browser* browser,
                                            const GURL& url,
                                            content::PageTransition transition);

// Creates a new tab with the already-created WebContents 'new_contents'.
// The window for the added contents will be reparented correctly when this
// method returns.  If |disposition| is NEW_POPUP, |pos| should hold the
// initial position. If |was_blocked| is non-NULL, then |*was_blocked| will be
// set to true if the popup gets blocked, and left unchanged otherwise.
void AddWebContents(Browser* browser,
                    content::WebContents* source_contents,
                    content::WebContents* new_contents,
                    WindowOpenDisposition disposition,
                    const gfx::Rect& initial_pos,
                    bool user_gesture,
                    bool* was__blocked);

// Closes the specified WebContents in the specified Browser. If
// |add_to_history| is true, an entry in the historical tab database is created.
void CloseWebContents(Browser* browser,
                      content::WebContents* contents,
                      bool add_to_history);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TABSTRIP_H_
