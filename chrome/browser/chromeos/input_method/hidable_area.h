// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_HIDABLE_AREA_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_HIDABLE_AREA_H_

#include "base/memory/scoped_ptr.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace input_method {

// HidableArea is used as an area to place optional information that can be
// turned displaying off if it is unnecessary.
class HidableArea : public views::View {
 public:
  HidableArea();
  virtual ~HidableArea();

  // Sets the content view.
  void SetContents(views::View* contents);

  // Shows the content.
  void Show();

  // Hides the content.
  void Hide();

  // Returns whether the content is already set and shown.
  bool IsShown() const;

  // Returns the content.
  views::View* contents() {
    return contents_.get();
  }

 private:
  scoped_ptr<views::View> contents_;
  scoped_ptr<views::View> place_holder_;

  DISALLOW_COPY_AND_ASSIGN(HidableArea);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_HIDABLE_AREA_H_
