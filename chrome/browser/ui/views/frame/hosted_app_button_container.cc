// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/extensions/hosted_app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/box_layout.h"

HostedAppButtonContainer::AppMenuButton::AppMenuButton(
    BrowserView* browser_view,
    bool use_light)
    : views::MenuButton(base::string16(), this, false),
      browser_view_(browser_view) {
  SetInkDropMode(InkDropMode::ON);
  SkColor color = use_light ? SK_ColorWHITE : gfx::kChromeIconGrey;
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kBrowserToolsIcon, color));
  set_ink_drop_base_color(color);
}

HostedAppButtonContainer::AppMenuButton::~AppMenuButton() {}

void HostedAppButtonContainer::AppMenuButton::OnMenuButtonClicked(
    views::MenuButton* source,
    const gfx::Point& point,
    const ui::Event* event) {
  Browser* browser = browser_view_->browser();
  menu_ = std::make_unique<AppMenu>(browser, 0);
  menu_model_ = std::make_unique<HostedAppMenuModel>(browser_view_, browser);
  menu_model_->Init();
  menu_->Init(menu_model_.get());

  menu_->RunMenu(this);
}

HostedAppButtonContainer::HostedAppButtonContainer(BrowserView* browser_view,
                                                   bool use_light)
    : app_menu_button_(new AppMenuButton(browser_view, use_light)) {
  auto layout =
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout.release());
  AddChildView(app_menu_button_);
}

HostedAppButtonContainer::~HostedAppButtonContainer() {}
