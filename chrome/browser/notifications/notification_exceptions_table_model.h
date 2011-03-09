// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_EXCEPTIONS_TABLE_MODEL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_EXCEPTIONS_TABLE_MODEL_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/remove_rows_table_model.h"
#include "content/common/notification_observer.h"

class NotificationExceptionsTableModel : public RemoveRowsTableModel,
                                         public NotificationObserver {
 public:
  explicit NotificationExceptionsTableModel(
      DesktopNotificationService* service);
  virtual ~NotificationExceptionsTableModel();

  // Overridden from RemoveRowsTableModel:
  virtual bool CanRemoveRows(const Rows& rows) const;
  virtual void RemoveRows(const Rows& rows);
  virtual void RemoveAll();

  // Overridden from TableModel:
  virtual int RowCount() OVERRIDE;
  virtual string16 GetText(int row, int column_id) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  struct Entry;

  void LoadEntries();

  DesktopNotificationService* service_;

  typedef std::vector<Entry> EntriesVector;
  EntriesVector entries_;

  // We use this variable to prevent ourselves from handling further changes
  // that we ourselves caused.
  bool updates_disabled_;
  NotificationRegistrar registrar_;
  ui::TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationExceptionsTableModel);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_EXCEPTIONS_TABLE_MODEL_H_
