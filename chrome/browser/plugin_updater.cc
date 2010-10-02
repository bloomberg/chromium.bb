// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_updater.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/plugins/plugin_group.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugininfo.h"

PluginUpdater::PluginUpdater() : enable_internal_pdf_(true) {
}

DictionaryValue* PluginUpdater::CreatePluginFileSummary(
    const WebPluginInfo& plugin) {
  DictionaryValue* data = new DictionaryValue();
  data->SetString("path", plugin.path.value());
  data->SetString("name", plugin.name);
  data->SetString("version", plugin.version);
  data->SetBoolean("enabled", plugin.enabled);
  return data;
}

// static
ListValue* PluginUpdater::GetPluginGroupsData() {
  NPAPI::PluginList::PluginMap plugin_groups;
  NPAPI::PluginList::Singleton()->GetPluginGroups(true, &plugin_groups);

  // Construct DictionaryValues to return to the UI
  ListValue* plugin_groups_data = new ListValue();
  for (NPAPI::PluginList::PluginMap::const_iterator it =
       plugin_groups.begin();
       it != plugin_groups.end();
       ++it) {
    plugin_groups_data->Append(it->second->GetDataForUI());
  }
  return plugin_groups_data;
}

void PluginUpdater::EnablePluginGroup(bool enable, const string16& group_name) {
  if (PluginGroup::IsPluginNameDisabledByPolicy(group_name))
    enable = false;
  if (NPAPI::PluginList::Singleton()->EnableGroup(enable, group_name)) {
    NotificationService::current()->Notify(
        NotificationType::PLUGIN_ENABLE_STATUS_CHANGED,
        Source<PluginUpdater>(this),
        NotificationService::NoDetails());
  }
}

void PluginUpdater::EnablePluginFile(bool enable,
                                     const FilePath::StringType& path) {
  FilePath file_path(path);
  if (enable && !PluginGroup::IsPluginPathDisabledByPolicy(file_path))
    NPAPI::PluginList::Singleton()->EnablePlugin(file_path);
  else
    NPAPI::PluginList::Singleton()->DisablePlugin(file_path);

  NotificationService::current()->Notify(
      NotificationType::PLUGIN_ENABLE_STATUS_CHANGED,
      Source<PluginUpdater>(this),
      NotificationService::NoDetails());
}

void PluginUpdater::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  DCHECK_EQ(NotificationType::PREF_CHANGED, type.value);
  const std::string* pref_name = Details<std::string>(details).ptr();
  if (!pref_name) {
    NOTREACHED();
    return;
  }
  if (*pref_name == prefs::kPluginsPluginsBlacklist) {
    PrefService* pref_service = Source<PrefService>(source).ptr();
    const ListValue* list =
        pref_service->GetList(prefs::kPluginsPluginsBlacklist);
    DisablePluginsFromPolicy(list);
  }
}

void PluginUpdater::DisablePluginsFromPolicy(const ListValue* plugin_names) {
  // Generate the set of unique disabled plugin patterns from the disabled
  // plugins list.
  std::set<string16> policy_disabled_plugin_patterns;
  if (plugin_names) {
    ListValue::const_iterator end(plugin_names->end());
    for (ListValue::const_iterator current(plugin_names->begin());
         current != end; ++current) {
      string16 plugin_name;
      if ((*current)->GetAsString(&plugin_name)) {
        policy_disabled_plugin_patterns.insert(plugin_name);
      }
    }
  }
  PluginGroup::SetPolicyDisabledPluginPatterns(policy_disabled_plugin_patterns);

  NotificationService::current()->Notify(
      NotificationType::PLUGIN_ENABLE_STATUS_CHANGED,
      Source<PluginUpdater>(this),
      NotificationService::NoDetails());
}

void PluginUpdater::DisablePluginGroupsFromPrefs(Profile* profile) {
  bool update_internal_dir = false;
  bool update_preferences = false;
  FilePath last_internal_dir =
  profile->GetPrefs()->GetFilePath(prefs::kPluginsLastInternalDirectory);
  FilePath cur_internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &cur_internal_dir) &&
      cur_internal_dir != last_internal_dir) {
    update_internal_dir = true;
    profile->GetPrefs()->SetFilePath(
        prefs::kPluginsLastInternalDirectory, cur_internal_dir);
  }

  if (!enable_internal_pdf_) {
    // This DCHECK guards against us disabling/enabling the pdf plugin more than
    // once without renaming the flag that tells us whether we can enable it
    // automatically.  Each time we disable the plugin by default after it was
    // enabled by default, we need to rename that flag.
    DCHECK(!profile->GetPrefs()->GetBoolean(prefs::kPluginsEnabledInternalPDF));
  }

  bool found_internal_pdf = false;
  bool force_enable_internal_pdf = false;
  string16 pdf_group_name;
  FilePath pdf_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path);
  FilePath::StringType pdf_path_str = pdf_path.value();
  if (enable_internal_pdf_ &&
      !profile->GetPrefs()->GetBoolean(prefs::kPluginsEnabledInternalPDF)) {
    // We switched to the internal pdf plugin being on by default, and so we
    // need to force it to be enabled.  We only want to do it this once though,
    // i.e. we don't want to enable it again if the user disables it afterwards.
    profile->GetPrefs()->SetBoolean(prefs::kPluginsEnabledInternalPDF, true);
    force_enable_internal_pdf = true;
  }

  if (ListValue* saved_plugins_list =
          profile->GetPrefs()->GetMutableList(prefs::kPluginsPluginsList)) {
    for (ListValue::const_iterator it = saved_plugins_list->begin();
         it != saved_plugins_list->end();
         ++it) {
      if (!(*it)->IsType(Value::TYPE_DICTIONARY)) {
        LOG(WARNING) << "Invalid entry in " << prefs::kPluginsPluginsList;
        continue;  // Oops, don't know what to do with this item.
      }

      DictionaryValue* plugin = static_cast<DictionaryValue*>(*it);
      string16 group_name;
      bool enabled = true;
      plugin->GetBoolean("enabled", &enabled);

      FilePath::StringType path;
      // The plugin list constains all the plugin files in addition to the
      // plugin groups.
      if (plugin->GetString("path", &path)) {
        // Files have a path attribute, groups don't.
        FilePath plugin_path(path);
        if (update_internal_dir &&
            FilePath::CompareIgnoreCase(plugin_path.DirName().value(),
                last_internal_dir.value()) == 0) {
          // If the internal plugin directory has changed and if the plugin
          // looks internal, update its path in the prefs.
          plugin_path = cur_internal_dir.Append(plugin_path.BaseName());
          path = plugin_path.value();
          plugin->SetString("path", path);
        }

        if (FilePath::CompareIgnoreCase(path, pdf_path_str) == 0) {
          found_internal_pdf = true;
          plugin->GetString("name", &pdf_group_name);
          if (!enabled && force_enable_internal_pdf) {
            enabled = true;
            plugin->SetBoolean("enabled", true);
            update_preferences = true;  // Can't modify the list during looping.
          }
        }
        if (!enabled)
          NPAPI::PluginList::Singleton()->DisablePlugin(plugin_path);
      } else if (!enabled && plugin->GetString("name", &group_name)) {
        // Don't disable this group if it's for the pdf plugin and we just
        // forced it on.
        if (force_enable_internal_pdf && pdf_group_name == group_name)
          continue;

        // Otherwise this is a list of groups.
        EnablePluginGroup(false, group_name);
      }
    }
  }

  // Build the set of policy-disabled plugin patterns once and cache it.
  // Don't do this in the constructor, there's no profile available there.
  const ListValue* plugin_blacklist =
      profile->GetPrefs()->GetList(prefs::kPluginsPluginsBlacklist);
  DisablePluginsFromPolicy(plugin_blacklist);

  if (!enable_internal_pdf_ && !found_internal_pdf) {
    // The internal PDF plugin is disabled by default, and the user hasn't
    // overridden the default.
    NPAPI::PluginList::Singleton()->DisablePlugin(pdf_path);
  }

  if (update_preferences)
    UpdatePreferences(profile);
}

void PluginUpdater::UpdatePreferences(Profile* profile) {
  ListValue* plugins_list = profile->GetPrefs()->GetMutableList(
      prefs::kPluginsPluginsList);
  plugins_list->Clear();

  FilePath internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir))
    profile->GetPrefs()->SetFilePath(prefs::kPluginsLastInternalDirectory,
                                     internal_dir);

  // Add the plugin files.
  std::vector<WebPluginInfo> plugins;
  NPAPI::PluginList::Singleton()->GetPlugins(false, &plugins);
  for (std::vector<WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end();
       ++it) {
    plugins_list->Append(CreatePluginFileSummary(*it));
  }

  // Add the groups as well.
  NPAPI::PluginList::PluginMap plugin_groups;
  NPAPI::PluginList::Singleton()->GetPluginGroups(false, &plugin_groups);
  for (NPAPI::PluginList::PluginMap::iterator it = plugin_groups.begin();
       it != plugin_groups.end(); ++it) {
    // Don't save preferences for vulnerable pugins.
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOutdatedPlugins) ||
        !it->second->IsVulnerable()) {
      plugins_list->Append(it->second->GetSummary());
    }
  }
}

/*static*/
PluginUpdater* PluginUpdater::GetPluginUpdater() {
  return Singleton<PluginUpdater>::get();
}
