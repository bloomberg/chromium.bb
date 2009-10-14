// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/browser_extender.h"

#include <algorithm>

#include "app/theme_provider.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/chromeos/main_menu.h"
#include "chrome/browser/chromeos/status_area_view.h"
#include "chrome/browser/views/frame/browser_frame_gtk.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"

BrowserExtender::BrowserExtender(BrowserView* browser_view)
    : browser_view_(browser_view),
      main_menu_(new views::ImageButton(this)) {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserExtender, public:

void BrowserExtender::Init() {
  ThemeProvider* theme_provider =
      browser_view_->frame()->GetThemeProviderForFrame();
  SkBitmap* image = theme_provider->GetBitmapNamed(IDR_MAIN_MENU_BUTTON);
  main_menu_->SetImage(views::CustomButton::BS_NORMAL, image);
  main_menu_->SetImage(views::CustomButton::BS_HOT, image);
  main_menu_->SetImage(views::CustomButton::BS_PUSHED, image);
  browser_view_->AddChildView(main_menu_);

  status_area_ = new StatusAreaView(browser_view_->browser());
  browser_view_->AddChildView(status_area_);
  status_area_->Init();

  // TODO(oshima): set the context menu controller to NonClientFrameView.
  InitSystemMenu();
}

gfx::Rect BrowserExtender::Layout(const gfx::Rect& bounds) {
  if (bounds.IsEmpty()) {
    // skip if there is no space to layout.
    return bounds;
  }
  // Layout main menu before tab strip.
  gfx::Size main_menu_size = main_menu_->GetPreferredSize();
  main_menu_->SetBounds(bounds.x(), bounds.y(),
                        main_menu_size.width(), bounds.height());

  // Layout status area after tab strip.
  gfx::Size status_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(bounds.x() + bounds.width() - status_size.width(),
                          bounds.y(), status_size.width(),
                          status_size.height());
  int width = bounds.width() - main_menu_size.width() - status_size.width();
  return gfx::Rect(bounds.x() + main_menu_size.width(),
                   bounds.y(),
                   std::max(0, width),  // in case there is no space left.
                   bounds.height());
}

bool BrowserExtender::NonClientHitTest(const gfx::Point& point) {
  gfx::Point point_in_main_menu_coords(point);
  views::View::ConvertPointToView(browser_view_->GetParent(), main_menu_,
                                  &point_in_main_menu_coords);

  gfx::Point point_in_status_area_coords(point);
  views::View::ConvertPointToView(browser_view_->GetParent(), status_area_,
                                  &point_in_status_area_coords);

  return main_menu_->HitTest(point_in_main_menu_coords) ||
      status_area_->HitTest(point_in_status_area_coords);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserExtender, private:

void BrowserExtender::InitSystemMenu() {
  system_menu_contents_.reset(new views::SimpleMenuModel(browser_view_));
  system_menu_contents_->AddItemWithStringId(IDC_RESTORE_TAB, IDS_RESTORE_TAB);
  system_menu_contents_->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_TASK_MANAGER,
                                             IDS_TASK_MANAGER);
  system_menu_menu_.reset(new views::Menu2(system_menu_contents_.get()));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserExtender, views::ButtonListener implemenation:

void BrowserExtender::ButtonPressed(views::Button* sender,
                                    const views::Event& event) {
  MainMenu::Show(browser_view_->browser());
}

///////////////////////////////////////////////////////////////////////////////
// BrowserExtender, views::ContextMenuController implementation:

void BrowserExtender::ShowContextMenu(views::View* source, int x, int y,
                                      bool is_mouse_gesture) {
  system_menu_menu_->RunMenuAt(gfx::Point(x, y), views::Menu2::ALIGN_TOPLEFT);
}

