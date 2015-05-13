// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/android/android_ui.h"

#include "components/view_manager/public/cpp/view.h"
#include "mandoline/ui/browser/browser.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/gfx/geometry/rect.h"

namespace mandoline {

class Browser;

AndroidUI::AndroidUI(Browser* browser, mojo::ApplicationImpl* application_impl)
    : browser_(browser),
      application_impl_(application_impl),
      root_(nullptr),
      content_(nullptr) {}

AndroidUI::~AndroidUI() {
  root_->RemoveObserver(this);
}

void AndroidUI::Init(mojo::View* root) {
  root_ = root;
  root_->AddObserver(this);

  browser_->content()->SetBounds(root_->bounds());
}

void AndroidUI::OnURLChanged() {
}

void AndroidUI::OnViewBoundsChanged(mojo::View* view,
                                    const mojo::Rect& old_bounds,
                                    const mojo::Rect& new_bounds) {
  content_->SetBounds(
      *mojo::Rect::From(gfx::Rect(0, 0, new_bounds.width, new_bounds.height)));
}

// static
BrowserUI* BrowserUI::Create(Browser* browser,
                             mojo::ApplicationImpl* application_impl) {
  return new AndroidUI(browser, application_impl);
}

}  // namespace mandoline
