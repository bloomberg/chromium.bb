// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DATABASE_INFO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DATABASE_INFO_VIEW_H_
#pragma once

#include "views/view.h"
#include "chrome/browser/browsing_data_database_helper.h"

namespace views {
class Label;
class Textfield;
}

///////////////////////////////////////////////////////////////////////////////
// DatabaseInfoView
//
//  Responsible for displaying a tabular grid of Database information.
class DatabaseInfoView : public views::View {
 public:
  DatabaseInfoView();
  virtual ~DatabaseInfoView();

  // Update the display from the specified Database info.
  void SetDatabaseInfo(
      const BrowsingDataDatabaseHelper::DatabaseInfo& database_info);

  // Clears the cookie display to indicate that no or multiple databases are
  // selected.
  void ClearDatabaseDisplay();

  // Enables or disables the database property text fields.
  void EnableDatabaseDisplay(bool enabled);

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(
      bool is_add, views::View* parent, views::View* child);

 private:
  // Set up the view layout.
  void Init();

  // Individual property labels.
  views::Textfield* name_value_field_;
  views::Textfield* description_value_field_;
  views::Textfield* size_value_field_;
  views::Textfield* last_modified_value_field_;

  DISALLOW_COPY_AND_ASSIGN(DatabaseInfoView);
};


#endif  // CHROME_BROWSER_UI_VIEWS_DATABASE_INFO_VIEW_H_
