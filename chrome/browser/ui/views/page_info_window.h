// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_WINDOW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_WINDOW_H_
#pragma once

#include "content/browser/tab_contents/navigation_entry.h"

class BrowserView;
class Profile;
class GURL;

namespace browser {

void ShowPageInfoBubble(BrowserView* browser_view,
                        Profile* profile,
                        const GURL& url,
                        const NavigationEntry::SSLStatus& ssl,
                        bool show_history);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_WINDOW_H_
