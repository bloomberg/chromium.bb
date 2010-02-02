// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_EXCEPTIONS_TABLE_MODEL_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_EXCEPTIONS_TABLE_MODEL_H_

#include <string>

#include "app/table_model.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/browser/host_content_settings_map.h"

class ContentExceptionsTableModel : public TableModel {
 public:
  ContentExceptionsTableModel(HostContentSettingsMap* map,
                              ContentSettingsType content_type);

  HostContentSettingsMap* map() const { return map_; }
  ContentSettingsType content_type() const { return content_type_; }

  const HostContentSettingsMap::HostSettingPair& entry_at(int index) {
    return entries_[index];
  }

  // Adds a new exception on the map and table model.
  void AddException(const std::string& host, ContentSetting setting);

  // Removes the exception at the specified index from both the map and model.
  void RemoveException(int row);

  // Removes all the exceptions from both the map and model.
  void RemoveAll();

  // Returns the index of the specified exception given a host, or -1 if there
  // is no exception for the specified host.
  int IndexOfExceptionByHost(const std::string& host);

  // TableModel overrides:
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(TableModelObserver* observer);

 private:
  HostContentSettingsMap* map_;
  ContentSettingsType content_type_;
  HostContentSettingsMap::SettingsForOneType entries_;
  TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ContentExceptionsTableModel);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_EXCEPTIONS_TABLE_MODEL_H_
