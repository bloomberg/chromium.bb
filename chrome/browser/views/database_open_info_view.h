// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_DATABASE_OPEN_INFO_VIEW_H_
#define CHROME_BROWSER_VIEWS_DATABASE_OPEN_INFO_VIEW_H_

#include <string>
#include <vector>

#include "base/string16.h"
#include "views/view.h"

namespace views {
class Label;
class Textfield;
}

///////////////////////////////////////////////////////////////////////////////
// DatabaseOpenInfoView
//
//  Responsible for displaying a tabular grid of Database information when
//  prompting for permission to open a new database.
class DatabaseOpenInfoView : public views::View {
 public:
  DatabaseOpenInfoView();
  virtual ~DatabaseOpenInfoView();

  // Update the display from the specified Database data.
  void SetFields(const std::string& host,
                 const string16& database_name);

  // Enables or disables the local storate property text fields.
  void EnableDisplay(bool enabled);

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(
      bool is_add, views::View* parent, views::View* child);

 private:
  // Set up the view layout
  void Init();

  // Individual property labels
  views::Textfield* host_value_field_;
  views::Textfield* database_name_value_field_;

  DISALLOW_COPY_AND_ASSIGN(DatabaseOpenInfoView);
};


#endif  // CHROME_BROWSER_VIEWS_DATABASE_OPEN_INFO_VIEW_H_

