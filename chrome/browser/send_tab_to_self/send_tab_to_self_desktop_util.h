// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_

#include <string>
#include "url/gurl.h"

class GURL;
class Profile;

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
  kShowDeviceList = 2,
  kMaxValue = kShowDeviceList,
};

namespace send_tab_to_self {

const char kOmniboxIcon[] = "OmniboxIcon";
const char kContentMenu[] = "ContentMenu";
const char kLinkMenu[] = "LinkMenu";
const char kOmniboxMenu[] = "OmniboxMenu";
const char kTabMenu[] = "TabMenu";

enum SendTabToSelfMenuType { kTab, kOmnibox, kContent, kLink };

// Adds a new entry to SendTabToSelfModel when user clicks a target device.
void CreateNewEntry(content::WebContents* tab,
                    const std::string& target_device_name,
                    const std::string& target_device_guid,
                    const GURL& link_url = GURL());

// Adds a new entry to SendTabToSelfModel when user clicks the single valid
// device. Will be called when GetValidDeviceCount() == 1.
void ShareToSingleTarget(content::WebContents* tab,
                         const GURL& link_url = GURL());

// Gets the icon for send tab to self menu item.
gfx::ImageSkia* GetImageSkia();

// Gets the image for send tab to self notification.
const gfx::Image GetImageForNotification();

// Records whether the user click to send a tab or link when send tab to self
// entry point is shown.
void RecordSendTabToSelfClickResult(const std::string& entry_point,
                                    SendTabToSelfClickResult state);

// Records the count of valid devices when user sees the device list.
void RecordSendTabToSelfDeviceCount(const std::string& entry_point,
                                    const int& device_count);

// Gets the count of valid device number.
int GetValidDeviceCount(Profile* profile);

// Gets the name of the single valid device. Will be called when
// GetValidDeviceCount() == 1.
std::string GetSingleTargetDeviceName(Profile* profile);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_
