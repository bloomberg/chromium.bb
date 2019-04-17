// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_

#include <string>
#include "url/gurl.h"

class GURL;

namespace content {
class WebContents;
}

namespace gfx {
class Image;
class ImageSkia;
}

// State of the send tab to self option in the context menu.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SendTabToSelfClickResult {
  kShowItem = 0,
  kClickItem = 1,
  kMaxValue = kClickItem,
};

namespace send_tab_to_self {

const char kContentMenu[] = "ContentMenu";
const char kLinkMenu[] = "LinkMenu";
const char kOmniboxMenu[] = "OmniboxMenu";
const char kTabMenu[] = "TabMenu";

// Add a new entry to SendTabToSelfModel when user click "Share to your
// devices" option.
void CreateNewEntry(content::WebContents* tab, const GURL& link_url = GURL());

// Get the icon for send tab to self menu item.
gfx::ImageSkia* GetImageSkia();

// Get the image for send tab to self notification.
const gfx::Image GetImageForNotification();

// Record whether the user click to send a tab or link when send tab to self
// entry point is shown in the context menu.
void RecordSendTabToSelfClickResult(std::string context_menu,
                                    SendTabToSelfClickResult state);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_
