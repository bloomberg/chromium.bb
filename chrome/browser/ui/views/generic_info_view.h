// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GENERIC_INFO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GENERIC_INFO_VIEW_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "ui/views/view.h"

namespace views {
class GridLayout;
class Label;
class Textfield;
}

// GenericInfoView, displays a tabular grid of read-only textual information,
// <name, value> pairs. The fixed number of rows must be known at the time of
// construction.
class GenericInfoView : public views::View {
 public:
  // Constructs a info view with |number_of_rows| and populated with
  // empty strings.
  explicit GenericInfoView(int number_of_rows);

  // Constructs a info view with |number_of_rows|, and populates
  // the name column with localized strings having the given
  // |name_string_ids|. The array of ids should contain |number_of_rows|
  // values and should remain valid for the life of the view.
  GenericInfoView(int number_of_rows, const int name_string_ids[]);

  // The following methods should only be called after
  // the view has been added to a view hierarchy.
  void SetNameByStringId(int row, int id);
  void SetName(int row, const string16& name);
  void SetValue(int row, const string16& value);
  void ClearValues() {
    const string16 kEmptyString;
    for (int i = 0; i < number_of_rows_; ++i)
      SetValue(i, kEmptyString);
  }

 protected:
  // views::View override
  virtual void ViewHierarchyChanged(
      bool is_add, views::View* parent, views::View* child);

 private:
  FRIEND_TEST_ALL_PREFIXES(GenericInfoViewTest, GenericInfoView);

  void InitGenericInfoView();
  void AddRow(int layout_id, views::GridLayout* layout,
              views::Label* name, views::Textfield* value);

  const int number_of_rows_;
  const int* name_string_ids_;
  scoped_array<views::Label*> name_views_;
  scoped_array<views::Textfield*> value_views_;

  DISALLOW_COPY_AND_ASSIGN(GenericInfoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GENERIC_INFO_VIEW_H_

