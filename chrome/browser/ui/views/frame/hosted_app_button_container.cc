// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/extensions/hosted_app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/native_widget_aura.h"

namespace {

// Padding around content setting icons.
constexpr int kContentSettingIconInteriorPadding = 4;

class HostedAppToolbarActionsBar : public ToolbarActionsBar {
 public:
  using ToolbarActionsBar::ToolbarActionsBar;

  size_t GetIconCount() const override {
    // Only show an icon when an extension action is popped out due to
    // activation, and none otherwise.
    return popped_out_action() ? 1 : 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HostedAppToolbarActionsBar);
};

}  // namespace

HostedAppButtonContainer::AppMenuButton::AppMenuButton(
    BrowserView* browser_view)
    : views::MenuButton(base::string16(), this, false),
      browser_view_(browser_view) {
  SetInkDropMode(InkDropMode::ON);
  // This name is guaranteed not to change during the lifetime of this button.
  // Get the app name only, aka "Google Docs" instead of "My Doc - Google Docs",
  // because the menu applies to the entire app.
  base::string16 app_name = base::UTF8ToUTF16(
      browser_view->browser()->hosted_app_controller()->GetAppShortName());
  SetAccessibleName(app_name);
  SetTooltipText(
      l10n_util::GetStringFUTF16(IDS_HOSTED_APPMENU_TOOLTIP, app_name));
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
      app_menu_button_(new AppMenuButton(browser_view)),
      browser_actions_container_(
          new BrowserActionsContainer(browser_view->browser(),
                                      nullptr,
                                      this,
                                      false /* interactive */)) {
  DCHECK(browser_view_);
  auto layout =
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(std::move(layout));

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

  AddChildView(browser_actions_container_);

  app_menu_button_->SetIconColor(active_icon_color);
  AddChildView(app_menu_button_);

  browser_view_->SetButtonProvider(this);
}

HostedAppButtonContainer::~HostedAppButtonContainer() {}

void HostedAppButtonContainer::RefreshContentSettingViews() {
  for (auto* v : content_setting_views_)
    v->Update();
}

void HostedAppButtonContainer::SetPaintAsActive(bool active) {
  for (auto* v : content_setting_views_)
    v->SetIconColor(active ? active_icon_color_ : inactive_icon_color_);

  app_menu_button_->SetIconColor(active ? active_icon_color_
                                        : inactive_icon_color_);
}

content::WebContents* HostedAppButtonContainer::GetContentSettingWebContents() {
  return browser_view_->GetActiveWebContents();
}

ContentSettingBubbleModelDelegate*
HostedAppButtonContainer::GetContentSettingBubbleModelDelegate() {
  return browser_view_->browser()->content_setting_bubble_model_delegate();
}

void HostedAppButtonContainer::OnContentSettingImageBubbleShown(
    ContentSettingImageModel::ImageType type) const {
  UMA_HISTOGRAM_ENUMERATION(
      "HostedAppFrame.ContentSettings.ImagePressed", type,
      ContentSettingImageModel::ImageType::NUM_IMAGE_TYPES);
}

void HostedAppButtonContainer::ChildPreferredSizeChanged(views::View* child) {
  if (child != browser_actions_container_)
    return;

  PreferredSizeChanged();
}

void HostedAppButtonContainer::ChildVisibilityChanged(views::View* child) {
  // Changes to layout need to be taken into account by the frame view.
  PreferredSizeChanged();
}

views::MenuButton* HostedAppButtonContainer::GetOverflowReferenceView() {
  return app_menu_button_;
}

base::Optional<int> HostedAppButtonContainer::GetMaxBrowserActionsWidth()
    const {
  // Our maximum size is 1 icon so don't specify a pixel-width max here.
  return base::Optional<int>();
}

std::unique_ptr<ToolbarActionsBar>
HostedAppButtonContainer::CreateToolbarActionsBar(
    ToolbarActionsBarDelegate* delegate,
    Browser* browser,
    ToolbarActionsBar* main_bar) const {
  DCHECK_EQ(browser_view_->browser(), browser);
  return std::make_unique<HostedAppToolbarActionsBar>(delegate, browser,
                                                      main_bar);
}

BrowserActionsContainer*
HostedAppButtonContainer::GetBrowserActionsContainer() {
  return browser_actions_container_;
}

views::MenuButton* HostedAppButtonContainer::GetAppMenuButton() {
  return app_menu_button_;
}
