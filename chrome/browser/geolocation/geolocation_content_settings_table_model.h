// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONTENT_SETTINGS_TABLE_MODEL_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONTENT_SETTINGS_TABLE_MODEL_H_

#include <vector>

#include "app/table_model.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"

class GeolocationContentSettingsTableModel : public TableModel {
 public:
  explicit GeolocationContentSettingsTableModel(
      GeolocationContentSettingsMap* map);

  // Return whether the given row can be removed.  A parent with setting of
  // CONTENT_SETTING_DEFAULT can't be removed.
  bool CanRemoveException(int row) const;

  // Removes the exception at the specified index from the map.  If it is a
  // parent, the row in model will be updated to have CONTENT_SETTING_DEFAULT.
  // If it is the only child of a CONTENT_SETTING_DEFAULT parent, the parent
  // will be removed from the model too.
  void RemoveException(int row);

  // Removes all the exceptions from both the map and model.
  void RemoveAll();

  // TableModel overrides:
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(TableModelObserver* observer);

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

  DISALLOW_COPY_AND_ASSIGN(GeolocationContentSettingsTableModel);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONTENT_SETTINGS_TABLE_MODEL_H_
