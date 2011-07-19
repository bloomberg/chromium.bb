// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DATABASE_OPEN_INFO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DATABASE_OPEN_INFO_VIEW_H_
#pragma once

#include "base/string16.h"
#include "chrome/browser/ui/views/generic_info_view.h"

///////////////////////////////////////////////////////////////////////////////
// DatabaseOpenInfoView
//
//  Responsible for displaying a tabular grid of Database information when
//  prompting for permission to open a new database.
class DatabaseOpenInfoView : public GenericInfoView {
 public:
  DatabaseOpenInfoView();

  // Update the display from the specified Database data.
  void SetFields(const std::string& host,
                 const string16& database_name,
                 const string16& display_name,
                 unsigned long estimated_size);

 private:
  DISALLOW_COPY_AND_ASSIGN(DatabaseOpenInfoView);
};


#endif  // CHROME_BROWSER_UI_VIEWS_DATABASE_OPEN_INFO_VIEW_H_

