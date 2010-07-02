// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_UPDATER_H_
#define CHROME_BROWSER_PLUGIN_UPDATER_H_

#include <vector>
#include "base/basictypes.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/plugins/webplugininfo.h"

class Version;
class Profile;

// Hard-coded definitions of plugin groups.
typedef struct {
  const char* const name;  // Name of this group.
  const char* const name_matcher;  // Substring matcher for the plugin name.
  const char* const version_matcher_low;  // Matchers for the plugin version.
  const char* const version_matcher_high;
  const char* const min_version;  // Minimum secure version.
  const char* const update_url;  // Location of latest secure version.
} PluginGroupDefinition;


// A PluginGroup contains at least one WebPluginInfo.
// In addition, it knows if the plugin is critically vulnerable.
class PluginGroup {
 public:
  // Creates a PluginGroup from a PluginGroupDefinition.
  static PluginGroup* FromPluginGroupDefinition(
      const PluginGroupDefinition& definition);

  // Creates a PluginGroup from a WebPluginInfo -- when no hard-coded
  // definition is found.
  static PluginGroup* FromWebPluginInfo(const WebPluginInfo& wpi);

  // Creates a copy of this plugin group.
  PluginGroup* Copy();

  // Returns true if the given plugin matches this group.
  bool Match(const WebPluginInfo& plugin) const;

  // Adds the given plugin to this group. Provide the position of the
  // plugin as given by PluginList so we can display its priority.
  void AddPlugin(const WebPluginInfo& plugin, int position);

  // Enables/disables this group. This enables/disables all plugins in the
  // group.
  void Enable(bool enable);

  // Returns this group's name
  const string16 GetGroupName() const;

  // Returns a DictionaryValue with data to display in the UI.
  DictionaryValue* GetData() const;

  // Returns a DictionaryValue with data to save in the preferences.
  DictionaryValue* GetSummary() const;

  // Returns true if the latest plugin in this group has known
  // security problems.
  bool IsVulnerable() const;

 private:
  PluginGroup(const string16& group_name,
              const string16& name_matcher,
              const std::string& version_range_low,
              const std::string& version_range_high,
              const std::string& min_version,
              const std::string& update_url);

  string16 group_name_;
  string16 name_matcher_;
  std::string version_range_low_str_;
  std::string version_range_high_str_;
  scoped_ptr<Version> version_range_low_;
  scoped_ptr<Version> version_range_high_;
  string16 description_;
  std::string update_url_;
  bool enabled_;
  std::string min_version_str_;
  scoped_ptr<Version> min_version_;
  scoped_ptr<Version> max_version_;
  std::vector<WebPluginInfo> web_plugin_infos_;
  std::vector<int> web_plugin_positions_;

  DISALLOW_COPY_AND_ASSIGN(PluginGroup);
};

class PluginUpdater {
 public:
  // Returns the PluginUpdater singleton.
  static PluginUpdater* GetInstance();

  static const PluginGroupDefinition* GetPluginGroupDefinitions();
  static const size_t GetPluginGroupDefinitionsSize();

  // Get a list of all the Plugin groups.
  ListValue* GetPluginGroupsData();

  // Get a list of all the Plugin groups.
  ListValue* GetPluginGroupsSummary();

  // Enable or disable a plugin group.
  void EnablePluginGroup(bool enable, const string16& group_name);

  // Enable or disable a specific plugin file.
  void EnablePluginFile(bool enable, const FilePath::StringType& file_path);

  // Disable all plugin groups as defined by the user's preference file.
  void DisablePluginGroupsFromPrefs(Profile* profile);

  // Write the enable/disable status to the user's preference file.
  void UpdatePreferences(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<PluginUpdater>;

  PluginUpdater();
  ~PluginUpdater();

  void GetPluginGroups(std::vector<linked_ptr<PluginGroup> >* plugin_groups);

  DictionaryValue* CreatePluginFileSummary(const WebPluginInfo& plugin);

  std::vector<linked_ptr<PluginGroup> > plugin_group_definitions_;

  // Set to true iff the internal pdf plugin is enabled by default.
  static bool enable_internal_pdf_;

  DISALLOW_COPY_AND_ASSIGN(PluginUpdater);
};

#endif  // CHROME_BROWSER_PLUGIN_UPDATER_H_
