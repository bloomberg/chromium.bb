// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HARMONY_TEXTFIELD_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_HARMONY_TEXTFIELD_LAYOUT_H_

#include "base/strings/string16.h"

namespace views {
class ColumnSet;
class GridLayout;
class Textfield;
}  // namespace views

// Configures a three-column ColumnSet for the standard textfield stack layout
// with: {Label, Padding, Textfield}.
views::ColumnSet* ConfigureTextfieldStack(views::GridLayout* layout,
                                          int column_set_id);

// Adds a views::Label and Textfield to |column_set_id| in |layout|, which has
// been configured as a Textfield stack. The returned Textfield will be owned by
// the View hosting |layout|.
views::Textfield* AddFirstTextfieldRow(views::GridLayout* layout,
                                       const base::string16& label,
                                       int column_set_id);
views::Textfield* AddTextfieldRow(views::GridLayout* layout,
                                  const base::string16& label,
                                  int column_set_id);

#endif  // CHROME_BROWSER_UI_VIEWS_HARMONY_TEXTFIELD_LAYOUT_H_
