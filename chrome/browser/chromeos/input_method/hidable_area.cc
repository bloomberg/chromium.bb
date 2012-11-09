// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/hidable_area.h"

#include "ui/views/layout/fill_layout.h"

namespace chromeos {
namespace input_method {

HidableArea::HidableArea() {
  place_holder_.reset(new views::View);
  place_holder_->set_owned_by_client();  // Won't own

  // Initially show nothing.
  SetLayoutManager(new views::FillLayout);
  AddChildView(place_holder_.get());
}

HidableArea::~HidableArea() {
}

void HidableArea::SetContents(views::View* contents) {
  contents_.reset(contents);
  contents_->set_owned_by_client();  // Won't own
}

void HidableArea::Show() {
  if (contents_.get() && contents_->parent() != this) {
    RemoveAllChildViews(false);  // Don't delete child views.
    AddChildView(contents_.get());
  }
}

void HidableArea::Hide() {
  if (IsShown()) {
    RemoveAllChildViews(false);  // Don't delete child views.
    AddChildView(place_holder_.get());
  }
}

bool HidableArea::IsShown() const {
  return contents_.get() && contents_->parent() == this;
}

}  // namespace input_method
}  // namespace chromeos
