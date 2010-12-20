// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_EXCEPTIONS_TABLE_MODEL_H_
#define CHROME_BROWSER_PLUGIN_EXCEPTIONS_TABLE_MODEL_H_
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/remove_rows_table_model.h"
#include "chrome/common/notification_observer.h"
#include "webkit/plugins/npapi/plugin_list.h"

struct WebPluginInfo;

class PluginExceptionsTableModel : public RemoveRowsTableModel,
                                   public NotificationObserver {
 public:
  PluginExceptionsTableModel(HostContentSettingsMap* content_settings_map,
                             HostContentSettingsMap* otr_content_settings_map);
  virtual ~PluginExceptionsTableModel();

  // Load plugin exceptions from the HostContentSettingsMaps. You should call
  // this method after creating a new PluginExceptionsTableModel.
  void LoadSettings();

  // RemoveRowsTableModel methods:
  virtual bool CanRemoveRows(const Rows& rows) const;
  virtual void RemoveRows(const Rows& rows);
  virtual void RemoveAll();

  // TableModel methods:
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(TableModelObserver* observer);
  virtual bool HasGroups();
  virtual Groups GetGroups();
  virtual int GetGroupID(int row);

  // NotificationObserver methods:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // Subclasses can override this method for testing.
  virtual void GetPlugins(
      std::vector<webkit::npapi::PluginGroup>* plugin_groups);

 private:
  friend class PluginExceptionsTableModelTest;

  struct SettingsEntry {
    ContentSettingsPattern pattern;
    int plugin_id;
    ContentSetting setting;
    bool is_otr;
  };

  void ClearSettings();
  void ReloadSettings();

  scoped_refptr<HostContentSettingsMap> map_;
  scoped_refptr<HostContentSettingsMap> otr_map_;

  std::deque<SettingsEntry> settings_;
  std::deque<int> row_counts_;
  std::deque<std::string> resources_;
  TableModel::Groups groups_;

  NotificationRegistrar registrar_;
  bool updates_disabled_;
  TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(PluginExceptionsTableModel);
};

#endif  // CHROME_BROWSER_PLUGIN_EXCEPTIONS_TABLE_MODEL_H_
