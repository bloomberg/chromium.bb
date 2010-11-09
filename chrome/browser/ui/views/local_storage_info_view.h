// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCAL_STORAGE_INFO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCAL_STORAGE_INFO_VIEW_H_
#pragma once

#include "views/view.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"

namespace views {
class Label;
class Textfield;
}

///////////////////////////////////////////////////////////////////////////////
// LocalStorageInfoView
//
//  Responsible for displaying a tabular grid of Local Storage information.
class LocalStorageInfoView : public views::View {
 public:
  LocalStorageInfoView();
  virtual ~LocalStorageInfoView();

  // Update the display from the specified Local Storage info.
  void SetLocalStorageInfo(
      const BrowsingDataLocalStorageHelper::LocalStorageInfo&
      local_storage_info);

  // Clears the cookie display to indicate that no or multiple local storages
  // are selected.
  void ClearLocalStorageDisplay();

  // Enables or disables the local storate property text fields.
  void EnableLocalStorageDisplay(bool enabled);

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(
      bool is_add, views::View* parent, views::View* child);

 private:
  // Set up the view layout
  void Init();

  // Individual property labels
  views::Textfield* origin_value_field_;
  views::Textfield* size_value_field_;
  views::Textfield* last_modified_value_field_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageInfoView);
};


#endif  // CHROME_BROWSER_UI_VIEWS_LOCAL_STORAGE_INFO_VIEW_H_

