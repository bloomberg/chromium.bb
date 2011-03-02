// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/frame/keyboard_container_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/common/url_constants.h"
#include "content/browser/site_instance.h"

namespace {

// Make the provided view and all of its descendents unfocusable.
void MakeViewHierarchyUnfocusable(views::View* view) {
  view->SetFocusable(false);
  for (int i = 0; i < view->child_count(); ++i) {
    MakeViewHierarchyUnfocusable(view->GetChildViewAt(i));
  }
}

}  // namepsace

///////////////////////////////////////////////////////////////////////////////
// KeyboardContainerView, public:

KeyboardContainerView::KeyboardContainerView(Profile* profile)
    : dom_view_(new DOMView) {
  GURL keyboard_url(chrome::kChromeUIKeyboardURL);
  dom_view_->Init(profile,
      SiteInstance::CreateSiteInstanceForURL(profile, keyboard_url));
  dom_view_->LoadURL(keyboard_url);

  dom_view_->SetVisible(true);
  AddChildView(dom_view_);
}

KeyboardContainerView::~KeyboardContainerView() {
}

void KeyboardContainerView::Layout() {
  // TODO(bryeung): include a border between the keyboard and the client view
  dom_view_->SetBounds(0, 0, width(), height());
}

void KeyboardContainerView::ViewHierarchyChanged(bool is_add,
                                                 View* parent,
                                                 View* child) {
  if (is_add)
    MakeViewHierarchyUnfocusable(child);
}
