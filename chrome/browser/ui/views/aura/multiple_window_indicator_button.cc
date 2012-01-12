// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/multiple_window_indicator_button.h"

#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/border.h"

MultipleWindowIndicatorButton::MultipleWindowIndicatorButton(
    StatusAreaButton::Delegate* delegate)
    : StatusAreaButton(delegate, NULL) {
  SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUSBAR_MULTIPLE_WINDOW));
  UpdateVisiblity();
  BrowserList::AddObserver(this);
}

MultipleWindowIndicatorButton::~MultipleWindowIndicatorButton() {
  BrowserList::RemoveObserver(this);
}

void MultipleWindowIndicatorButton::OnBrowserAdded(const Browser* browser) {
  UpdateVisiblity();
}

void MultipleWindowIndicatorButton::OnBrowserRemoved(const Browser* browser) {
  UpdateVisiblity();
}

void MultipleWindowIndicatorButton::UpdateVisiblity() {
  bool visible = false;
  int count = 0;
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    if ((*it)->is_type_tabbed()) {
      ++count;
      if (count >= 2) {
        visible = true;
        break;
      }
    }
  }
  SetVisible(visible);
}

