// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_EXCEPTIONS_TABLE_MODEL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_EXCEPTIONS_TABLE_MODEL_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/remove_rows_table_model.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"

class NotificationExceptionsTableModel : public RemoveRowsTableModel {
 public:
  explicit NotificationExceptionsTableModel(
      DesktopNotificationService* service);

  // RemoveRowsTableModel overrides:
  virtual bool CanRemoveRows(const Rows& rows) const;
  virtual void RemoveRows(const Rows& rows);
  virtual void RemoveAll();

  // TableModel overrides:
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(TableModelObserver* observer);

 private:
  struct Entry {
    Entry(const GURL& origin, ContentSetting setting);
    bool operator<(const Entry& b) const;

    GURL origin;
    ContentSetting setting;
  };

  DesktopNotificationService* service_;

  typedef std::vector<Entry> EntriesVector;
  EntriesVector entries_;

  TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationExceptionsTableModel);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_EXCEPTIONS_TABLE_MODEL_H_

