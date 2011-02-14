// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_updater.h"

#include <set>
#include <string>

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pepper_plugin_registry.h"
#include "chrome/common/pref_names.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugininfo.h"

// How long to wait to save the plugin enabled information, which might need to
// go to disk.
#define kPluginUpdateDelayMs (60 * 1000)

PluginUpdater::PluginUpdater()
    : notify_pending_(false) {
}

DictionaryValue* PluginUpdater::CreatePluginFileSummary(
    const webkit::npapi::WebPluginInfo& plugin) {
  DictionaryValue* data = new DictionaryValue();
  data->SetString("path", plugin.path.value());
  data->SetString("name", plugin.name);
  data->SetString("version", plugin.version);
  data->SetBoolean("enabled", webkit::npapi::IsPluginEnabled(plugin));
  return data;
}

// static
ListValue* PluginUpdater::GetPluginGroupsData() {
  std::vector<webkit::npapi::PluginGroup> plugin_groups;
  webkit::npapi::PluginList::Singleton()->GetPluginGroups(true, &plugin_groups);

  // Construct DictionaryValues to return to the UI
  ListValue* plugin_groups_data = new ListValue();
  for (size_t i = 0; i < plugin_groups.size(); ++i) {
    plugin_groups_data->Append(plugin_groups[i].GetDataForUI());
  }
  return plugin_groups_data;
}

void PluginUpdater::EnablePluginGroup(bool enable, const string16& group_name) {
  webkit::npapi::PluginList::Singleton()->EnableGroup(enable, group_name);
  NotifyPluginStatusChanged();
}

void PluginUpdater::EnablePlugin(bool enable,
                                 const FilePath::StringType& path) {
  FilePath file_path(path);
  if (enable)
    webkit::npapi::PluginList::Singleton()->EnablePlugin(file_path);
  else
    webkit::npapi::PluginList::Singleton()->DisablePlugin(file_path);

  NotifyPluginStatusChanged();
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
  webkit::npapi::PluginGroup::SetPolicyDisabledPluginPatterns(
      policy_disabled_plugin_patterns);

  NotifyPluginStatusChanged();
}

void PluginUpdater::DisablePluginGroupsFromPrefs(Profile* profile) {
  bool update_internal_dir = false;
  FilePath last_internal_dir =
  profile->GetPrefs()->GetFilePath(prefs::kPluginsLastInternalDirectory);
  FilePath cur_internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &cur_internal_dir) &&
      cur_internal_dir != last_internal_dir) {
    update_internal_dir = true;
    profile->GetPrefs()->SetFilePath(
        prefs::kPluginsLastInternalDirectory, cur_internal_dir);
  }

  bool force_enable_internal_pdf = false;
  bool internal_pdf_enabled = false;
  string16 pdf_group_name = ASCIIToUTF16(PepperPluginRegistry::kPDFPluginName);
  FilePath pdf_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path);
  FilePath::StringType pdf_path_str = pdf_path.value();
  if (!profile->GetPrefs()->GetBoolean(prefs::kPluginsEnabledInternalPDF)) {
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
          if (!enabled && force_enable_internal_pdf) {
            enabled = true;
            plugin->SetBoolean("enabled", true);
          }

          internal_pdf_enabled = enabled;
        }

        if (!enabled)
          webkit::npapi::PluginList::Singleton()->DisablePlugin(plugin_path);
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

  if (force_enable_internal_pdf || internal_pdf_enabled) {
    // See http://crbug.com/50105 for background.
    EnablePluginGroup(false, ASCIIToUTF16(
        webkit::npapi::PluginGroup::kAdobeReaderGroupName));
  }

  if (force_enable_internal_pdf) {
    // We want to save this, but doing so requires loading the list of plugins,
    // so do it after a minute as to not impact startup performance.  Note that
    // plugins are loaded after 30s by the metrics service.
    UpdatePreferences(profile, kPluginUpdateDelayMs);
  }
}

void PluginUpdater::UpdatePreferences(Profile* profile, int delay_ms) {
  BrowserThread::PostDelayedTask(
    BrowserThread::FILE,
    FROM_HERE,
    NewRunnableFunction(
        &PluginUpdater::GetPreferencesDataOnFileThread, profile), delay_ms);
}

void PluginUpdater::GetPreferencesDataOnFileThread(void* profile) {
  std::vector<webkit::npapi::WebPluginInfo> plugins;
  webkit::npapi::PluginList::Singleton()->GetPlugins(false, &plugins);

  std::vector<webkit::npapi::PluginGroup> groups;
  webkit::npapi::PluginList::Singleton()->GetPluginGroups(false, &groups);

  BrowserThread::PostTask(
    BrowserThread::UI,
    FROM_HERE,
    NewRunnableFunction(&PluginUpdater::OnUpdatePreferences,
                        static_cast<Profile*>(profile),
                        plugins, groups));
}

void PluginUpdater::OnUpdatePreferences(
    Profile* profile,
    const std::vector<webkit::npapi::WebPluginInfo>& plugins,
    const std::vector<webkit::npapi::PluginGroup>& groups) {
  ListValue* plugins_list = profile->GetPrefs()->GetMutableList(
      prefs::kPluginsPluginsList);
  plugins_list->Clear();

  FilePath internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir))
    profile->GetPrefs()->SetFilePath(prefs::kPluginsLastInternalDirectory,
                                     internal_dir);

  // Add the plugin files.
  for (size_t i = 0; i < plugins.size(); ++i) {
    DictionaryValue* summary = CreatePluginFileSummary(plugins[i]);
    // If the plugin is disabled only by policy don't store this state in the
    // user pref store.
    if (plugins[i].enabled ==
        webkit::npapi::WebPluginInfo::USER_ENABLED_POLICY_DISABLED) {
      summary->SetBoolean("enabled", true);
    }
    bool enabled_val;
    summary->GetBoolean("enabled", &enabled_val);
    plugins_list->Append(summary);
  }

  // Add the groups as well.
  for (size_t i = 0; i < groups.size(); ++i) {
      DictionaryValue* summary = groups[i].GetSummary();
      // If the plugin is disabled only by policy don't store this state in the
      // user pref store.
      if (!groups[i].Enabled() &&
          webkit::npapi::PluginGroup::IsPluginNameDisabledByPolicy(
              groups[i].GetGroupName()))
        summary->SetBoolean("enabled", true);
      plugins_list->Append(summary);
  }
}

void PluginUpdater::NotifyPluginStatusChanged() {
  if (notify_pending_)
    return;
  notify_pending_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableFunction(&PluginUpdater::OnNotifyPluginStatusChanged));
}

void PluginUpdater::OnNotifyPluginStatusChanged() {
  GetInstance()->notify_pending_ = false;
  NotificationService::current()->Notify(
      NotificationType::PLUGIN_ENABLE_STATUS_CHANGED,
      Source<PluginUpdater>(GetInstance()),
      NotificationService::NoDetails());
}

/*static*/
PluginUpdater* PluginUpdater::GetInstance() {
  return Singleton<PluginUpdater>::get();
}
