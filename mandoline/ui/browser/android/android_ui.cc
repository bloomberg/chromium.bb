// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/android/android_ui.h"

#include "components/view_manager/public/cpp/view.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/gfx/geometry/rect.h"

namespace mandoline {

AndroidUI::AndroidUI(Browser* browser, mojo::Shell* shell)
    : browser_(browser),
      shell_(shell),
      root_(nullptr),
      content_(nullptr) {}

AndroidUI::~AndroidUI() {
  root_->RemoveObserver(this);
}

void AndroidUI::Init(mojo::View* root, mojo::View* content) {
  root_ = root;
  root_->AddObserver(this);
  content_ = content;

  content_->SetBounds(root_->bounds());
}

void AndroidUI::OnViewBoundsChanged(mojo::View* view,
                                    const mojo::Rect& old_bounds,
                                    const mojo::Rect& new_bounds) {
  content_->SetBounds(
      *mojo::Rect::From(gfx::Rect(0, 0, new_bounds.width, new_bounds.height)));
}

// static
BrowserUI* BrowserUI::Create(Browser* browser, mojo::Shell* shell) {
  return new AndroidUI(browser, shell);
}

}  // namespace mandoline
