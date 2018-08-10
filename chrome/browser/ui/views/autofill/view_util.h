// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_VIEW_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_VIEW_UTIL_H_

#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
}  // namespace views

namespace autofill {

// Defines a title view with an icon, a separator, and a label, to be used
// by dialogs that need to present the Google Pay logo with a separator and
// custom horizontal padding.
class TitleWithIconAndSeparatorView : public views::View {
 public:
  explicit TitleWithIconAndSeparatorView(const base::string16& window_title);
  ~TitleWithIconAndSeparatorView() override = default;

 private:
  // views::View:
  gfx::Size GetMinimumSize() const override;
};

// Creates and returns a small Textfield intended to be used for CVC entry.
views::Textfield* CreateCvcTextfield();

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_VIEW_UTIL_H_
