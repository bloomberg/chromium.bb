// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_UPDATER_H_
#define CHROME_BROWSER_PLUGIN_UPDATER_H_
#pragma once

#include  <map>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/linked_ptr.h"
#include "base/singleton.h"
#include "chrome/common/notification_observer.h"

class DictionaryValue;
class ListValue;
class NotificationDetails;
class NotificationSource;
class PluginGroup;
class Profile;
struct WebPluginInfo;

class PluginUpdater : public NotificationObserver {
 public:
  typedef std::map<std::string, linked_ptr<PluginGroup> > PluginMap;

  // Get a map from identifier to plugin group for all plugin groups.
  void GetPluginGroups(PluginMap* plugin_groups);

  // Get a list of all the plugin groups. The caller should take ownership
  // of the returned ListValue.
  ListValue* GetPluginGroupsData();

  // Enable or disable a plugin group.
  void EnablePluginGroup(bool enable, const string16& group_name);

  // Enable or disable a specific plugin file.
  void EnablePluginFile(bool enable, const FilePath::StringType& file_path);

  // Disable all plugin groups as defined by the user's preference file.
  void DisablePluginGroupsFromPrefs(Profile* profile);

  // Disable all plugins groups that are known to be outdated, according to
  // the information hardcoded in PluginGroup, to make sure that they can't
  // be loaded on a web page and instead show a UI to update to the latest
  // version.
  void DisableOutdatedPluginGroups();

  // Write the enable/disable status to the user's preference file.
  void UpdatePreferences(Profile* profile);

  // NotificationObserver method overrides
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  static PluginUpdater* GetPluginUpdater();

 private:
  PluginUpdater();
  virtual ~PluginUpdater() {}

  // Note: if you change this to false from true, you must update
  // kPluginsEnabledInternalPDF to be a new name (i.e. add 2, 3, 4...) at end.
  bool enable_internal_pdf_;

  DictionaryValue* CreatePluginFileSummary(const WebPluginInfo& plugin);

  // Force plugins to be disabled due to policy. |plugins| contains
  // the list of StringValues of the names of the policy-disabled plugins.
  void DisablePluginsFromPolicy(const ListValue* plugin_names);

  // Needed to allow singleton instantiation using private constructor.
  friend struct DefaultSingletonTraits<PluginUpdater>;

  DISALLOW_COPY_AND_ASSIGN(PluginUpdater);
};

#endif  // CHROME_BROWSER_PLUGIN_UPDATER_H_
