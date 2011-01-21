// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_exceptions_table_model.h"

#include "base/auto_reset.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"

PluginExceptionsTableModel::PluginExceptionsTableModel(
    HostContentSettingsMap* content_settings_map,
    HostContentSettingsMap* otr_content_settings_map)
    : map_(content_settings_map),
      otr_map_(otr_content_settings_map),
      updates_disabled_(false),
      observer_(NULL) {
  registrar_.Add(this, NotificationType::CONTENT_SETTINGS_CHANGED,
                 NotificationService::AllSources());
}

PluginExceptionsTableModel::~PluginExceptionsTableModel() {}

bool PluginExceptionsTableModel::CanRemoveRows(const Rows& rows) const {
  return !rows.empty();
}

void PluginExceptionsTableModel::RemoveRows(const Rows& rows) {
  AutoReset<bool> tmp(&updates_disabled_, true);
  bool reload_all = false;
  // Iterate in reverse over the rows to get the indexes right.
  for (Rows::const_reverse_iterator it = rows.rbegin();
       it != rows.rend(); ++it) {
    DCHECK_LT(*it, settings_.size());
    SettingsEntry& entry = settings_[*it];
    HostContentSettingsMap* map = entry.is_otr ? otr_map_ : map_;
    map->SetContentSetting(entry.pattern,
                           CONTENT_SETTINGS_TYPE_PLUGINS,
                           resources_[entry.plugin_id],
                           CONTENT_SETTING_DEFAULT);
    settings_.erase(settings_.begin() + *it);
    row_counts_[entry.plugin_id]--;
    if (!reload_all) {
      // If we remove the last exception for a plugin, recreate all groups
      // to get correct IDs.
      if (row_counts_[entry.plugin_id] == 0)  {
        reload_all = true;
      } else {
        observer_->OnItemsRemoved(*it, 1);
      }
    }
  }
  if (reload_all) {
    // This also notifies the observer.
    ReloadSettings();
  }
}

void PluginExceptionsTableModel::RemoveAll() {
  AutoReset<bool> tmp(&updates_disabled_, true);
  map_->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS);
  if (otr_map_)
    otr_map_->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS);

  ClearSettings();
  if (observer_)
    observer_->OnModelChanged();
}

int PluginExceptionsTableModel::RowCount() {
  return settings_.size();
}

string16 PluginExceptionsTableModel::GetText(int row, int column_id) {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, static_cast<int>(settings_.size()));
  SettingsEntry& entry = settings_[row];
  switch (column_id) {
    case IDS_EXCEPTIONS_PATTERN_HEADER:
    case IDS_EXCEPTIONS_HOSTNAME_HEADER:
      return UTF8ToUTF16(entry.pattern.AsString());

    case IDS_EXCEPTIONS_ACTION_HEADER:
      switch (entry.setting) {
        case CONTENT_SETTING_ALLOW:
          return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ALLOW_BUTTON);
        case CONTENT_SETTING_BLOCK:
          return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_BLOCK_BUTTON);
        default:
          NOTREACHED();
      }
      break;

    default:
      NOTREACHED();
  }

  return string16();
}

bool PluginExceptionsTableModel::HasGroups() {
  return true;
}

void PluginExceptionsTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

ui::TableModel::Groups PluginExceptionsTableModel::GetGroups() {
  return groups_;
}

int PluginExceptionsTableModel::GetGroupID(int row) {
  DCHECK_LT(row, static_cast<int>(settings_.size()));
  return settings_[row].plugin_id;
}

void PluginExceptionsTableModel::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  if (!updates_disabled_)
    ReloadSettings();
}

void PluginExceptionsTableModel::ClearSettings() {
  settings_.clear();
  groups_.clear();
  row_counts_.clear();
  resources_.clear();
}

void PluginExceptionsTableModel::GetPlugins(
    std::vector<webkit::npapi::PluginGroup>* plugin_groups) {
  webkit::npapi::PluginList::Singleton()->GetPluginGroups(false, plugin_groups);
}

void PluginExceptionsTableModel::LoadSettings() {
  int group_id = 0;
  std::vector<webkit::npapi::PluginGroup> plugins;
  GetPlugins(&plugins);
  for (size_t i = 0; i < plugins.size(); ++i) {
    std::string plugin = plugins[i].identifier();
    HostContentSettingsMap::SettingsForOneType settings;
    map_->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS,
                                plugin,
                                &settings);
    HostContentSettingsMap::SettingsForOneType otr_settings;
    if (otr_map_) {
      otr_map_->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS,
                                      plugin,
                                      &otr_settings);
    }
    string16 title = plugins[i].GetGroupName();
    for (HostContentSettingsMap::SettingsForOneType::iterator setting_it =
             settings.begin(); setting_it != settings.end(); ++setting_it) {
      SettingsEntry entry = {
        setting_it->first,
        group_id,
        setting_it->second,
        false
      };
      settings_.push_back(entry);
    }
    for (HostContentSettingsMap::SettingsForOneType::iterator setting_it =
             otr_settings.begin();
         setting_it != otr_settings.end(); ++setting_it) {
      SettingsEntry entry = {
        setting_it->first,
        group_id,
        setting_it->second,
        true
      };
      settings_.push_back(entry);
    }
    int num_plugins = settings.size() + otr_settings.size();
    if (num_plugins > 0) {
      Group group = { title, group_id++ };
      groups_.push_back(group);
      resources_.push_back(plugin);
      row_counts_.push_back(num_plugins);
    }
  }
}

void PluginExceptionsTableModel::ReloadSettings() {
  ClearSettings();
  LoadSettings();

  if (observer_)
    observer_->OnModelChanged();
}

