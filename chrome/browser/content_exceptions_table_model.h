// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_EXCEPTIONS_TABLE_MODEL_H_
#define CHROME_BROWSER_CONTENT_EXCEPTIONS_TABLE_MODEL_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "ui/base/models/table_model.h"

class ContentExceptionsTableModel : public ui::TableModel {
 public:
  ContentExceptionsTableModel(HostContentSettingsMap* map,
                              HostContentSettingsMap* off_the_record_map,
                              ContentSettingsType content_type);
  virtual ~ContentExceptionsTableModel();

  HostContentSettingsMap* map() const { return map_; }
  HostContentSettingsMap* off_the_record_map() const {
    return off_the_record_map_;
  }
  ContentSettingsType content_type() const { return content_type_; }

  bool entry_is_off_the_record(int index) {
    return index >= static_cast<int>(entries_.size());
  }

  const HostContentSettingsMap::PatternSettingPair& entry_at(int index) {
    return (entry_is_off_the_record(index) ?
            off_the_record_entries_[index - entries_.size()] : entries_[index]);
  }

  // Adds a new exception on the map and table model.
  void AddException(const ContentSettingsPattern& pattern,
                    ContentSetting setting,
                    bool is_off_the_record);

  // Removes the exception at the specified index from both the map and model.
  void RemoveException(int row);

  // Removes all the exceptions from both the map and model.
  void RemoveAll();

  // Returns the index of the specified exception given a host, or -1 if there
  // is no exception for the specified host.
  int IndexOfExceptionByPattern(const ContentSettingsPattern& pattern,
                                bool is_off_the_record);

  // TableModel overrides:
  virtual int RowCount() OVERRIDE;
  virtual string16 GetText(int row, int column_id) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;

 private:
  HostContentSettingsMap* map(bool is_off_the_record) {
    return is_off_the_record ? off_the_record_map_ : map_;
  }
  HostContentSettingsMap::SettingsForOneType& entries(bool is_off_the_record) {
    return is_off_the_record ? off_the_record_entries_ : entries_;
  }

  scoped_refptr<HostContentSettingsMap> map_;
  scoped_refptr<HostContentSettingsMap> off_the_record_map_;
  ContentSettingsType content_type_;
  HostContentSettingsMap::SettingsForOneType entries_;
  HostContentSettingsMap::SettingsForOneType off_the_record_entries_;
  ui::TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ContentExceptionsTableModel);
};

#endif  // CHROME_BROWSER_CONTENT_EXCEPTIONS_TABLE_MODEL_H_
