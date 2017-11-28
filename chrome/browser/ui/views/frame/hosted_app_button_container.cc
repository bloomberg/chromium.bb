// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/extensions/hosted_app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/native_widget_aura.h"

namespace {

// Padding around content setting icons.
constexpr int kContentSettingIconInteriorPadding = 4;

}  // namespace

HostedAppButtonContainer::AppMenuButton::AppMenuButton(
    BrowserView* browser_view)
    : views::MenuButton(base::string16(), this, false),
      browser_view_(browser_view) {
  SetInkDropMode(InkDropMode::ON);
}

HostedAppButtonContainer::AppMenuButton::~AppMenuButton() {}

void HostedAppButtonContainer::AppMenuButton::SetIconColor(SkColor color) {
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kBrowserToolsIcon, color));
  set_ink_drop_base_color(color);
}

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
                                                   SkColor active_icon_color,
                                                   SkColor inactive_icon_color)
    : browser_view_(browser_view),
      active_icon_color_(active_icon_color),
      inactive_icon_color_(inactive_icon_color),
      app_menu_button_(new AppMenuButton(browser_view)) {
  DCHECK(browser_view_);
  auto layout =
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout.release());

  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), this,
        views::NativeWidgetAura::GetWindowTitleFontList());
    image_view->SetIconColor(active_icon_color);
    image_view->set_next_element_interior_padding(
        kContentSettingIconInteriorPadding);
    image_view->SetVisible(false);
    image_view->disable_animation();
    content_setting_views_.push_back(image_view.get());
    AddChildView(image_view.release());
  }

  app_menu_button_->SetIconColor(active_icon_color);
  AddChildView(app_menu_button_);
}

HostedAppButtonContainer::~HostedAppButtonContainer() {}

content::WebContents* HostedAppButtonContainer::GetContentSettingWebContents() {
  return browser_view_->GetActiveWebContents();
}

ContentSettingBubbleModelDelegate*
HostedAppButtonContainer::GetContentSettingBubbleModelDelegate() {
  return browser_view_->browser()->content_setting_bubble_model_delegate();
}

void HostedAppButtonContainer::RefreshContentSettingViews() {
  for (auto* v : content_setting_views_)
    v->Update();
}

void HostedAppButtonContainer::ChildVisibilityChanged(views::View* child) {
  // Changes to layout need to be taken into account by the frame view.
  browser_view_->frame()->GetFrameView()->Layout();
}

void HostedAppButtonContainer::SetPaintAsActive(bool active) {
  for (auto* v : content_setting_views_)
    v->SetIconColor(active ? active_icon_color_ : inactive_icon_color_);

  app_menu_button_->SetIconColor(active ? active_icon_color_
                                        : inactive_icon_color_);
}
