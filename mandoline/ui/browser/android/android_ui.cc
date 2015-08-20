// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/android/android_ui.h"

#include "components/view_manager/public/cpp/view.h"
#include "mandoline/ui/browser/browser.h"
#include "mandoline/ui/browser/browser_manager.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/gfx/geometry/rect.h"

namespace mandoline {

AndroidUI::AndroidUI(mojo::ApplicationImpl* application_impl,
                     BrowserManager* manager)
    : browser_(application_impl, this),
      manager_(manager),
      application_impl_(application_impl),
      root_(nullptr) {}

AndroidUI::~AndroidUI() {
  root_->RemoveObserver(this);
}

void AndroidUI::Init(mojo::View* root) {
  root_ = root;
  root_->AddObserver(this);

  browser_.content()->SetBounds(root_->bounds());
}

void AndroidUI::LoadURL(const GURL& url) {
  browser_.LoadURL(url);
}

void AndroidUI::ViewManagerDisconnected()  {
  manager_->BrowserUIClosed(this);
}

void AndroidUI::EmbedOmnibox(mojo::ApplicationConnection* connection) {}
void AndroidUI::OnURLChanged() {}
void AndroidUI::LoadingStateChanged(bool loading) {}
void AndroidUI::ProgressChanged(double progress) {}

void AndroidUI::OnViewBoundsChanged(mojo::View* view,
                                    const mojo::Rect& old_bounds,
                                    const mojo::Rect& new_bounds) {
  browser_.content()->SetBounds(
      *mojo::Rect::From(gfx::Rect(0, 0, new_bounds.width, new_bounds.height)));
}

// static
BrowserUI* BrowserUI::Create(mojo::ApplicationImpl* application_impl,
                             BrowserManager* manager) {
  return new AndroidUI(application_impl, manager);
}

}  // namespace mandoline
