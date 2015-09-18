// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/web_app_left_header_view_ash.h"

#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/toolbar/toolbar_model.h"
#include "content/public/browser/navigation_entry.h"
#include "grit/ash_resources.h"
#include "ui/views/layout/box_layout.h"

// static
const char WebAppLeftHeaderView::kViewClassName[] = "WebAppLeftHeaderView";

WebAppLeftHeaderView::WebAppLeftHeaderView(BrowserView* browser_view)
    : browser_view_(browser_view) {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

  back_button_ =
      new ash::FrameCaptionButton(this, ash::CAPTION_BUTTON_ICON_BACK);
  back_button_->SetImages(
      ash::CAPTION_BUTTON_ICON_BACK, ash::FrameCaptionButton::ANIMATE_NO,
      IDR_AURA_WINDOW_CONTROL_ICON_BACK, IDR_AURA_WINDOW_CONTROL_BACKGROUND_H,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_P);
  AddChildView(back_button_);

  location_icon_ =
      new ash::FrameCaptionButton(this, ash::CAPTION_BUTTON_ICON_LOCATION);
  AddChildView(location_icon_);

  Update();
}

WebAppLeftHeaderView::~WebAppLeftHeaderView() {
}

void WebAppLeftHeaderView::Update() {
  int icon_resource = browser_view_->browser()->toolbar_model()->GetIcon();
  location_icon_->SetImages(ash::CAPTION_BUTTON_ICON_LOCATION,
                            ash::FrameCaptionButton::ANIMATE_NO, icon_resource,
                            IDR_AURA_WINDOW_CONTROL_BACKGROUND_H,
                            IDR_AURA_WINDOW_CONTROL_BACKGROUND_P);

  back_button_->SetState(
      chrome::IsCommandEnabled(browser_view_->browser(), IDC_BACK)
          ? views::Button::STATE_NORMAL
          : views::Button::STATE_DISABLED);
}

void WebAppLeftHeaderView::SetPaintAsActive(bool active) {
  // TODO(benwells): Check that the disabled and inactive states should be
  // drawn in the same way.
  back_button_->set_paint_as_active(
      active && chrome::IsCommandEnabled(browser_view_->browser(), IDC_BACK));
  location_icon_->set_paint_as_active(active);
}

views::View* WebAppLeftHeaderView::GetLocationIconView() const {
  return location_icon_;
}

const char* WebAppLeftHeaderView::GetClassName() const {
  return kViewClassName;
}

void WebAppLeftHeaderView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  if (sender == back_button_)
    chrome::ExecuteCommand(browser_view_->browser(), IDC_BACK);
  else if (sender == location_icon_)
    ShowWebsiteSettings();
  else
    NOTREACHED();
}

void WebAppLeftHeaderView::ShowWebsiteSettings() const {
  content::WebContents* tab = browser_view_->GetActiveWebContents();
  if (!tab)
    return;

  // Important to use GetVisibleEntry to match what's showing in the title area.
  content::NavigationEntry* nav_entry = tab->GetController().GetVisibleEntry();
  // The visible entry can be NULL in the case of window.open("").
  if (!nav_entry)
    return;

  SecurityStateModel* security_model = SecurityStateModel::FromWebContents(tab);
  DCHECK(security_model);

  chrome::ShowWebsiteSettings(browser_view_->browser(), tab,
                              nav_entry->GetURL(),
                              security_model->GetSecurityInfo());
}
