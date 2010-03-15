// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/account_creation_view.h"

#include "app/gfx/canvas.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "views/border.h"

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, public:
AccountCreationView::AccountCreationView() {
}

AccountCreationView::~AccountCreationView() {
}

void AccountCreationView::Init() {
  // Use rounded rect background.
  views::Border* border = chromeos::CreateWizardBorder(
      &chromeos::BorderDefinition::kScreenBorder);
  set_border(border);
}

void AccountCreationView::InitDOM(Profile* profile,
                                  SiteInstance* site_instance) {
  DOMView::Init(profile, site_instance);
}

void AccountCreationView::SetTabContentsDelegate(
    TabContentsDelegate* delegate) {
  tab_contents_->set_delegate(delegate);
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, views::View implementation:
void AccountCreationView::Paint(gfx::Canvas* canvas) {
  PaintBorder(canvas);
  DOMView::Paint(canvas);
}
