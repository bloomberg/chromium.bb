// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PLUGIN_GROUP_H_
#define CHROME_COMMON_PLUGIN_GROUP_H_
#pragma once

#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"

class DictionaryValue;
class FilePath;
class Version;
struct WebPluginInfo;

template <typename T>
class linked_ptr;

// Hard-coded definitions of plugin groups.
struct PluginGroupDefinition {
  const char* name;  // Name of this group.
  const char* name_matcher;  // Substring matcher for the plugin name.
  const char* version_matcher_low;  // Matchers for the plugin version.
  const char* version_matcher_high;
  const char* min_version;  // Minimum secure version.
  const char* update_url;  // Location of latest secure version.
};

// A PluginGroup can match a range of versions of a specific plugin (as defined
// by matching a substring of its name).
// It contains all WebPluginInfo structs (at least one) matching its definition.
// In addition, it knows about a security "baseline", i.e. the minimum version
// of a plugin that is needed in order not to exhibit known security
// vulnerabilities.

class PluginGroup {
 public:
  // Creates a PluginGroup from a PluginGroupDefinition.
  static PluginGroup* FromPluginGroupDefinition(
      const PluginGroupDefinition& definition);

  ~PluginGroup();

  // Creates a PluginGroup from a WebPluginInfo -- when no hard-coded
  // definition is found.
  static PluginGroup* FromWebPluginInfo(const WebPluginInfo& wpi);

  // Find a plugin group matching |info| in the list of hardcoded plugins and
  // returns a copy of it if found, or a new group matching exactly this plugin
  // otherwise.
  static PluginGroup* FindHardcodedPluginGroup(const WebPluginInfo& info);

  // Configures the set of plugin name patterns for disabling plugins via
  // enterprise configuration management.
  static void SetPolicyDisabledPluginPatterns(const std::set<string16>& set);

  // Tests to see if a plugin is on the blacklist using its name as
  // the lookup key.
  static bool IsPluginNameDisabledByPolicy(const string16& plugin_name);

  // Tests to see if a plugin is on the blacklist using its path as
  // the lookup key.
  static bool IsPluginPathDisabledByPolicy(const FilePath& plugin_path);

  // Find the PluginGroup matching a Plugin in a list of plugin groups. Returns
  // NULL if no matching PluginGroup is found.
  static PluginGroup* FindGroupMatchingPlugin(
      std::vector<linked_ptr<PluginGroup> >& plugin_groups,
      const WebPluginInfo& plugin);

  // Creates a copy of this plugin group.
  PluginGroup* Copy() {
    return new PluginGroup(group_name_, name_matcher_, version_range_low_str_,
                           version_range_high_str_, min_version_str_,
                           update_url_);
  }

  // Returns true if the given plugin matches this group.
  bool Match(const WebPluginInfo& plugin) const;

  // Adds the given plugin to this group. Provide the position of the
  // plugin as given by PluginList so we can display its priority.
  void AddPlugin(const WebPluginInfo& plugin, int position);

  // Enables/disables this group. This enables/disables all plugins in the
  // group.
  void Enable(bool enable);

  // Returns whether the plugin group is enabled or not.
  bool Enabled() const { return enabled_; }

  // Returns this group's name, or the filename without extension if the name
  // is empty.
  string16 GetGroupName() const;

  // Returns the description of the highest-priority plug-in in the group.
  const string16& description() const { return description_; }

  // Returns a DictionaryValue with data to display in the UI.
  DictionaryValue* GetDataForUI() const;

  // Returns a DictionaryValue with data to save in the preferences.
  DictionaryValue* GetSummary() const;

  // Returns the update URL.
  std::string GetUpdateURL() const { return update_url_; }

  // Returns true if the highest-priority plugin in this group has known
  // security problems.
  bool IsVulnerable() const;

  // Disables all plugins in this group that are older than the
  // minimum version.
  void DisableOutdatedPlugins();

 private:
  FRIEND_TEST_ALL_PREFIXES(PluginGroupTest, PluginGroupDefinition);

  static const PluginGroupDefinition* GetPluginGroupDefinitions();
  static size_t GetPluginGroupDefinitionsSize();

  PluginGroup(const string16& group_name,
              const string16& name_matcher,
              const std::string& version_range_low,
              const std::string& version_range_high,
              const std::string& min_version,
              const std::string& update_url);

  Version* CreateVersionFromString(const string16& version_string);

  // Set the description and version for this plugin group from the
  // given plug-in.
  void UpdateDescriptionAndVersion(const WebPluginInfo& plugin);

  // Updates the active plugin in the group. The active plugin is the first
  // enabled one, or if all plugins are disabled, simply the first one.
  void UpdateActivePlugin(const WebPluginInfo& plugin);

  static std::set<string16>* policy_disabled_plugin_patterns_;

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
  scoped_ptr<Version> version_;
  std::vector<WebPluginInfo> web_plugin_infos_;
  std::vector<int> web_plugin_positions_;

  DISALLOW_COPY_AND_ASSIGN(PluginGroup);
};

#endif  // CHROME_COMMON_PLUGIN_GROUP_H_
