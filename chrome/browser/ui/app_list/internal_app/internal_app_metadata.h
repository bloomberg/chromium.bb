// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_METADATA_H_
#define CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_METADATA_H_

#include <string>
#include <vector>

#include "ui/gfx/image/image_skia.h"

class Profile;
class GURL;

namespace app_list {

// The internal app's histogram name of the chrome search result. This is used
// for logging so do not change the order of this enum.
enum class InternalAppName {
  kKeyboardShortcutViewer = 0,
  kSettings = 1,
  kContinueReading = 2,
  kCamera = 3,
  kDiscover = 4,
  kMaxValue = kDiscover,
};

// Metadata about an internal app.
// Internal apps are these Chrome OS special apps, e.g. Settings, or these apps
// can run in Chrome OS directly, e.g. Keyboard Shortcut Viewer.
struct InternalApp {
  const char* app_id;

  // Name of the app.
  int name_string_resource_id = 0;

  int icon_resource_id = 0;

  // Can show as a suggested app.
  bool recommendable;

  // Can be searched.
  bool searchable;

  // Can show in launcher apps grid.
  bool show_in_launcher;

  InternalAppName internal_app_name;

  // The string used for search query in addition to the name.
  int searchable_string_resource_id = 0;
};

// Returns a list of Chrome OS internal apps, which are searchable in launcher
// for |profile|.
const std::vector<InternalApp>& GetInternalAppList(const Profile* profile);

// Returns InternalApp by |app_id|.
// Returns nullptr if |app_id| does not correspond to an internal app.
const InternalApp* FindInternalApp(const std::string& app_id);

// Returns true if |app_id| corresponds to an internal app.
bool IsInternalApp(const std::string& app_id);

// Returns the name of internal app.
// Returns empty string if |app_id| is invalid.
base::string16 GetInternalAppNameById(const std::string& app_id);

// Returns the app's icon resource id.
// Returns 0 if |app_id| is invalid.
int GetIconResourceIdByAppId(const std::string& app_id);

// Helper function to open internal apps.
void OpenInternalApp(const std::string& app_id,
                     Profile* profile,
                     int event_flags);

// Returns icon associated with the |resource_id|.
// Returns empty ImageSkia if |resource_id| is 0;
// |resource_size_in_dip| is the preferred size of the icon.
gfx::ImageSkia GetIconForResourceId(int resource_id, int resource_size_in_dip);

// Returns true if there is a recommendable foreign tab.
// If |title| is not nullptr, it will be replaced with the title of the foreign
// tab's last navigation.
// If |url| is not nullptr, it will be replaced with the url of the foreign
// tab's last navigation.
bool HasRecommendableForeignTab(Profile* profile,
                                base::string16* title,
                                GURL* url);

// Returns the InternalAppName of an internal app.
InternalAppName GetInternalAppNameByAppId(
    const std::string& app_id);

// Returns the number of internal apps which can show in launcher.
// If |apps_name| is not nullptr, it will be the concatenated string of these
// internal apps' name.
size_t GetNumberOfInternalAppsShowInLauncherForTest(std::string* apps_name,
                                                    const Profile* profile);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_METADATA_H_
