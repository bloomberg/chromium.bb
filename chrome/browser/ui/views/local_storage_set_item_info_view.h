// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCAL_STORAGE_SET_ITEM_INFO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCAL_STORAGE_SET_ITEM_INFO_VIEW_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
}

///////////////////////////////////////////////////////////////////////////////
// LocalStorageSetItemInfoView
//
//  Responsible for displaying a tabular grid of Local Storage information when
//  prompting for permission to set an item.
class LocalStorageSetItemInfoView : public views::View {
 public:
  LocalStorageSetItemInfoView();
  virtual ~LocalStorageSetItemInfoView();

  // Update the display from the specified Local Storage info.
  void SetFields(const std::string& host,
                 const string16& key,
                 const string16& value);

  // Clears the display to indicate that no or multiple local storages
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
  views::Textfield* host_value_field_;
  views::Textfield* key_value_field_;
  views::Textfield* value_value_field_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageSetItemInfoView);
};


#endif  // CHROME_BROWSER_UI_VIEWS_LOCAL_STORAGE_SET_ITEM_INFO_VIEW_H_
