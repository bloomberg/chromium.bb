// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_menu_item_factory.h"

#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace {
constexpr gfx::Insets kDefaultBorderInsets = gfx::Insets(12);
}  // namespace

void ConfigureBubbleMenuItem(views::Button* button, int button_id) {
  // Items within a menu should not show focus rings.
  button->SetInstallFocusRingOnFocus(false);
  button->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  button->GetInkDrop()->SetShowHighlightOnFocus(true);
  button->GetInkDrop()->SetHoverHighlightFadeDuration(base::TimeDelta());
  views::InstallRectHighlightPathGenerator(button);
  button->set_ink_drop_base_color(HoverButton::GetInkDropColor(button));
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetID(button_id);
}

std::unique_ptr<views::LabelButton> CreateBubbleMenuItem(
    int button_id,
    const base::string16& name,
    views::ButtonListener* listener) {
  auto button = std::make_unique<views::LabelButton>(
      listener, name, views::style::CONTEXT_BUTTON);

  ConfigureBubbleMenuItem(button.get(), button_id);

  button->SetButtonController(std::make_unique<HoverButtonController>(
      button.get(), listener,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          button.get())));
  button->SetBorder(views::CreateEmptyBorder(kDefaultBorderInsets));

  return button;
}

std::unique_ptr<views::ImageButton> CreateBubbleMenuItem(
    int button_id,
    views::ButtonListener* listener) {
  auto button = views::CreateVectorImageButton(listener);
  ConfigureBubbleMenuItem(button.get(), button_id);
  return button;
}
