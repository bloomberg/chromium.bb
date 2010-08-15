// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_EXCEPTIONS_TABLE_MODEL_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_EXCEPTIONS_TABLE_MODEL_H_
#pragma once

#include <vector>

#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/remove_rows_table_model.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"

class GeolocationExceptionsTableModel : public RemoveRowsTableModel {
 public:
  explicit GeolocationExceptionsTableModel(
      GeolocationContentSettingsMap* map);

  // RemoveRowsTableModel overrides:

  // Return whether the given set of rows can be removed.  A parent with setting
  // of CONTENT_SETTING_DEFAULT can't be removed unless all its children are
  // also being removed.
  virtual bool CanRemoveRows(const Rows& rows) const;

  // Removes the exceptions at the specified indexes.  If an exception is a
  // parent, and it has children, the row in model will be updated to have
  // CONTENT_SETTING_DEFAULT.  If it is the only child of a
  // CONTENT_SETTING_DEFAULT parent, the parent will be removed from the model
  // too.
  virtual void RemoveRows(const Rows& rows);

  // Removes all the exceptions from both the map and model.
  virtual void RemoveAll();

  // TableModel overrides:
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(TableModelObserver* observer);
  virtual int CompareValues(int row1, int row2, int column_id);

 private:
  struct Entry {
    Entry(const GURL& origin,
          const GURL& embedding_origin,
          ContentSetting setting);

    GURL origin;
    GURL embedding_origin;
    ContentSetting setting;
  };

  void AddEntriesForOrigin(
      const GURL& origin,
      const GeolocationContentSettingsMap::OneOriginSettings& settings);

  GeolocationContentSettingsMap* map_;

  typedef std::vector<Entry> EntriesVector;
  EntriesVector entries_;

  TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationExceptionsTableModel);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_EXCEPTIONS_TABLE_MODEL_H_
