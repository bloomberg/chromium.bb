// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_UPDATER_H_
#define CHROME_BROWSER_PLUGIN_UPDATER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/linked_ptr.h"
#include "base/singleton.h"

class ListValue;
class Profile;

namespace plugin_updater {

// Get a list of all the Plugin groups.
ListValue* GetPluginGroupsData();

// Enable or disable a plugin group.
void EnablePluginGroup(bool enable, const string16& group_name);

// Enable or disable a specific plugin file.
void EnablePluginFile(bool enable, const FilePath::StringType& file_path);

// Disable all plugin groups as defined by the user's preference file.
void DisablePluginGroupsFromPrefs(Profile* profile);

// Write the enable/disable status to the user's preference file.
void UpdatePreferences(Profile* profile);

}  // namespace plugin_updater

#endif  // CHROME_BROWSER_PLUGIN_UPDATER_H_
