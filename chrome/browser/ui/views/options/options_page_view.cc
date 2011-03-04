// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/options_page_view.h"

#include "chrome/browser/metrics/user_metrics.h"
#include "views/widget/widget.h"

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView

OptionsPageView::OptionsPageView(Profile* profile)
    : OptionsPageBase(profile),
      initialized_(false) {
}

OptionsPageView::~OptionsPageView() {
}

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView, views::View overrides:

void OptionsPageView::ViewHierarchyChanged(bool is_add,
                                           views::View* parent,
                                           views::View* child) {
  if (!initialized_ && is_add && GetWidget()) {
    // It is important that this only get done _once_ otherwise we end up
    // duplicating the view hierarchy when tabs are switched.
    initialized_ = true;
    InitControlLayout();
    NotifyPrefChanged(NULL);
  }
}

AccessibilityTypes::Role OptionsPageView::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_PAGETAB;
}
