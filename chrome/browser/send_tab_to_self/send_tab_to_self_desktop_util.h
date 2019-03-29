// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_

class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class ImageSkia;
}

namespace send_tab_to_self {

// Add a new entry to SendTabToSelfModel when user click "Share to my devices"
// option
// TODO(crbug.com/945386): Flip parameter order.
void CreateNewEntry(content::WebContents* tab, Profile* profile);

// Get the icon for send tab to self menu item.
gfx::ImageSkia* GetImageSkia();

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_
