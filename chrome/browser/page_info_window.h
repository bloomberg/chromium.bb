// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_WINDOW_H_
#define CHROME_BROWSER_PAGE_INFO_WINDOW_H_
#pragma once

#include "chrome/browser/tab_contents/navigation_entry.h"
#include "gfx/native_widget_types.h"

class Profile;
class GURL;

namespace browser {

// Shows the page info using the specified information.
// |url| is the url of the page/frame the info applies to, |ssl| is the SSL
// information for that page/frame.  If |show_history| is true, a section
// showing how many times that URL has been visited is added to the page info.
void ShowPageInfo(gfx::NativeWindow parent,
                  Profile* profile,
                  const GURL& url,
                  const NavigationEntry::SSLStatus& ssl,
                  bool show_history);

void ShowPageInfoBubble(gfx::NativeWindow parent,
                        Profile* profile,
                        const GURL& url,
                        const NavigationEntry::SSLStatus& ssl,
                        bool show_history);

}  // namespace browser

#endif  // CHROME_BROWSER_PAGE_INFO_WINDOW_H_
