// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/compact_navigation_bar.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/logging.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/back_forward_menu_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/theme_background.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/button_dropdown.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/controls/native/native_view_host.h"

namespace chromeos {

// Padding inside each button around the image.
static const int kInnerPadding = 1;

// Spacing between buttons (excluding left/right most margin)
static const int kHorizMargin = 3;

// Left side margin of the back button to align with the main menu.
static const int kBackButtonLeftMargin = 10;

// Right side margin of the forward button to align with the main menu.
static const int kForwardButtonRightMargin = 1;

// Preferred height.
static const int kPreferredHeight = 25;

////////////////////////////////////////////////////////////////////////////////
// CompactNavigationBar public:

CompactNavigationBar::CompactNavigationBar(::BrowserView* browser_view)
    : browser_view_(browser_view),
      initialized_(false) {
}

CompactNavigationBar::~CompactNavigationBar() {
}

void CompactNavigationBar::Init() {
  DCHECK(!initialized_);
  initialized_ = true;
  Browser* browser = browser_view_->browser();
  browser->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser->command_updater()->AddCommandObserver(IDC_FORWARD, this);

  back_menu_model_.reset(new BackForwardMenuModel(
      browser, BackForwardMenuModel::BACKWARD_MENU));
  forward_menu_model_.reset(new BackForwardMenuModel(
      browser, BackForwardMenuModel::FORWARD_MENU));

  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();

  back_ = new views::ButtonDropDown(this, back_menu_model_.get());
  back_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                     views::Event::EF_MIDDLE_BUTTON_DOWN);
  back_->set_tag(IDC_BACK);
  back_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_BACK));
  back_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_BACK));
  back_->SetImage(views::CustomButton::BS_NORMAL,
                  resource_bundle.GetBitmapNamed(IDR_COMPACTNAV_BACK));
  back_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                           views::ImageButton::ALIGN_MIDDLE);

  AddChildView(back_);

  bf_separator_ = new views::ImageView;
  bf_separator_->SetImage(
      resource_bundle.GetBitmapNamed(IDR_COMPACTNAV_SEPARATOR));
  AddChildView(bf_separator_);

  forward_ = new views::ButtonDropDown(this, forward_menu_model_.get());
  forward_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                        views::Event::EF_MIDDLE_BUTTON_DOWN);
  forward_->set_tag(IDC_FORWARD);
  forward_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_FORWARD));
  forward_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_FORWARD));
  forward_->SetImage(views::CustomButton::BS_NORMAL,
                     resource_bundle.GetBitmapNamed(IDR_COMPACTNAV_FORWARD));
  forward_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                              views::ImageButton::ALIGN_MIDDLE);
  AddChildView(forward_);

  set_background(new ThemeBackground(browser_view_));
}

gfx::Size CompactNavigationBar::GetPreferredSize() {
  int width = kBackButtonLeftMargin;
  width += back_->GetPreferredSize().width() + kInnerPadding * 2;
  width += kHorizMargin;
  width += bf_separator_->GetPreferredSize().width();
  width += kHorizMargin;
  width += forward_->GetPreferredSize().width() + kInnerPadding * 2;
  width += kForwardButtonRightMargin;
  return gfx::Size(width, kPreferredHeight);
}

void CompactNavigationBar::Layout() {
  if (!initialized_)
    return;

  // Layout forward/back buttons after entry views as follows:
  // [Back]|[Forward]
  int curx = kBackButtonLeftMargin;
  // "Back | Forward" section.
  gfx::Size button_size = back_->GetPreferredSize();
  button_size.set_width(button_size.width() + kInnerPadding * 2);
  back_->SetBounds(curx, 0, button_size.width(), height());
  curx += button_size.width() + kHorizMargin;

  button_size = bf_separator_->GetPreferredSize();
  bf_separator_->SetBounds(curx, 0, button_size.width(), height());
  curx += button_size.width() + kHorizMargin;

  button_size = forward_->GetPreferredSize();
  button_size.set_width(button_size.width() + kInnerPadding * 2);
  forward_->SetBounds(curx, 0, button_size.width(), height());
}

void CompactNavigationBar::Paint(gfx::Canvas* canvas) {
  PaintBackground(canvas);
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation.

void CompactNavigationBar::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  browser_view_->browser()->ExecuteCommandWithDisposition(
      sender->tag(),
      event_utils::DispositionFromEventFlags(sender->mouse_event_flags()));
}

////////////////////////////////////////////////////////////////////////////////
// CommandUpdater::CommandObserver implementation.
void CompactNavigationBar::EnabledStateChangedForCommand(int id, bool enabled) {
  switch (id) {
    case IDC_BACK:
      back_->SetEnabled(enabled);
      break;
    case IDC_FORWARD:
      forward_->SetEnabled(enabled);
      break;
  }
}

}  // namespace chromeos
