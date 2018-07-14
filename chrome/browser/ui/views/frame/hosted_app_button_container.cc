// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_menu_button.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/native_widget_aura.h"

namespace {

bool g_animation_disabled_for_testing = false;

constexpr base::TimeDelta kContentSettingsFadeInDuration =
    base::TimeDelta::FromMilliseconds(500);

class HostedAppToolbarActionsBar : public ToolbarActionsBar {
 public:
  using ToolbarActionsBar::ToolbarActionsBar;

  gfx::Insets GetIconAreaInsets() const override {
    // TODO(calamity): Unify these toolbar action insets with other clients once
    // all toolbar button sizings are consolidated. https://crbug.com/822967.
    return gfx::Insets(2);
  }

  size_t GetIconCount() const override {
    // Only show an icon when an extension action is popped out due to
    // activation, and none otherwise.
    return popped_out_action() ? 1 : 0;
  }

  int GetMinimumWidth() const override {
    // Allow the BrowserActionsContainer to collapse completely and be hidden
    return 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HostedAppToolbarActionsBar);
};

int HorizontalPaddingBetweenItems() {
  return views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
}

}  // namespace

class HostedAppButtonContainer::ContentSettingsContainer
    : public views::View,
      public ContentSettingImageView::Delegate {
 public:
  explicit ContentSettingsContainer(BrowserView* browser_view);
  ~ContentSettingsContainer() override = default;

  void UpdateContentSettingViewsVisibility() {
    for (auto* v : content_setting_views_)
      v->Update();
  }

  // Sets the color of the content setting icons.
  void SetIconColor(SkColor icon_color) {
    for (auto* v : content_setting_views_)
      v->SetIconColor(icon_color);
  }

  void FadeIn() {
    SetVisible(true);
    DCHECK_EQ(layer()->opacity(), 0);
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTransitionDuration(kContentSettingsFadeInDuration);
    layer()->SetOpacity(1);
  }

  const std::vector<ContentSettingImageView*>&
  GetContentSettingViewsForTesting() const {
    return content_setting_views_;
  }

 private:
  // views::View:
  void ChildVisibilityChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  // ContentSettingsImageView::Delegate:
  content::WebContents* GetContentSettingWebContents() override {
    return browser_view_->GetActiveWebContents();
  }
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override {
    return browser_view_->browser()->content_setting_bubble_model_delegate();
  }
  void OnContentSettingImageBubbleShown(
      ContentSettingImageModel::ImageType type) const override {
    UMA_HISTOGRAM_ENUMERATION(
        "HostedAppFrame.ContentSettings.ImagePressed", type,
        ContentSettingImageModel::ImageType::NUM_IMAGE_TYPES);
  }

  // Owned by the views hierarchy.
  std::vector<ContentSettingImageView*> content_setting_views_;

  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsContainer);
};

void HostedAppButtonContainer::DisableAnimationForTesting() {
  g_animation_disabled_for_testing = true;
}

views::View* HostedAppButtonContainer::GetContentSettingContainerForTesting() {
  return content_settings_container_;
}

const std::vector<ContentSettingImageView*>&
HostedAppButtonContainer::GetContentSettingViewsForTesting() const {
  return content_settings_container_->GetContentSettingViewsForTesting();
}

HostedAppButtonContainer::ContentSettingsContainer::ContentSettingsContainer(
    BrowserView* browser_view)
    : browser_view_(browser_view) {
  DCHECK(
      extensions::HostedAppBrowserController::IsForExperimentalHostedAppBrowser(
          browser_view->browser()));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  if (!g_animation_disabled_for_testing) {
    SetVisible(false);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetOpacity(0);
  }

  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), this,
        views::NativeWidgetAura::GetWindowTitleFontList());
    // Padding around content setting icons.
    constexpr int kContentSettingIconInteriorPadding = 4;
    image_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kContentSettingIconInteriorPadding)));
    image_view->disable_animation();
    content_setting_views_.push_back(image_view.get());
    AddChildView(image_view.release());
  }
}

HostedAppButtonContainer::HostedAppButtonContainer(BrowserView* browser_view,
                                                   SkColor active_icon_color,
                                                   SkColor inactive_icon_color)
    : browser_view_(browser_view),
      active_icon_color_(active_icon_color),
      inactive_icon_color_(inactive_icon_color),
      content_settings_container_(new ContentSettingsContainer(browser_view)),
      page_action_icon_container_view_(new PageActionIconContainerView(
          {PageActionIconType::kFind, PageActionIconType::kZoom},
          GetLayoutConstant(HOSTED_APP_PAGE_ACTION_ICON_SIZE),
          HorizontalPaddingBetweenItems(),
          browser_view->browser(),
          this,
          nullptr)),
      browser_actions_container_(
          new BrowserActionsContainer(browser_view->browser(),
                                      nullptr,
                                      this,
                                      false /* interactive */)),
      app_menu_button_(new HostedAppMenuButton(browser_view)) {
  DCHECK(browser_view_);
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal,
      gfx::Insets(0, HorizontalPaddingBetweenItems()),
      HorizontalPaddingBetweenItems());
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(std::move(layout));

  AddChildView(content_settings_container_);
  AddChildView(page_action_icon_container_view_);
  AddChildView(browser_actions_container_);
  AddChildView(app_menu_button_);

  UpdateIconsColor();

  browser_view_->SetToolbarButtonProvider(this);
  browser_view_->immersive_mode_controller()->AddObserver(this);
}

HostedAppButtonContainer::~HostedAppButtonContainer() {
  ImmersiveModeController* immersive_controller =
      browser_view_->immersive_mode_controller();
  if (immersive_controller)
    immersive_controller->RemoveObserver(this);
}

void HostedAppButtonContainer::UpdateContentSettingViewsVisibility() {
  content_settings_container_->UpdateContentSettingViewsVisibility();
}

void HostedAppButtonContainer::SetPaintAsActive(bool active) {
  if (paint_as_active_ == active)
    return;
  paint_as_active_ = active;
  UpdateIconsColor();
}

void HostedAppButtonContainer::UpdateIconsColor() {
  SkColor icon_color =
      paint_as_active_ ? active_icon_color_ : inactive_icon_color_;
  content_settings_container_->SetIconColor(icon_color);
  page_action_icon_container_view_->SetIconColor(icon_color);
  app_menu_button_->SetIconColor(icon_color);
}

void HostedAppButtonContainer::StartTitlebarAnimation(
    base::TimeDelta origin_text_slide_duration) {
  if (g_animation_disabled_for_testing ||
      browser_view_->immersive_mode_controller()->IsEnabled()) {
    return;
  }

  app_menu_button_->StartHighlightAnimation(origin_text_slide_duration);

  fade_in_content_setting_buttons_timer_.Start(
      FROM_HERE, origin_text_slide_duration, content_settings_container_,
      &ContentSettingsContainer::FadeIn);
}

void HostedAppButtonContainer::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void HostedAppButtonContainer::OnImmersiveRevealStarted() {
  // Cancel the content setting animation as icons need immediately show in
  // immersive mode.
  if (fade_in_content_setting_buttons_timer_.IsRunning()) {
    fade_in_content_setting_buttons_timer_.AbandonAndStop();
    content_settings_container_->SetVisible(true);
  }
}

void HostedAppButtonContainer::ChildVisibilityChanged(views::View* child) {
  // Changes to layout need to be taken into account by the frame view.
  PreferredSizeChanged();
}

const char* HostedAppButtonContainer::GetClassName() const {
  return "HostedAppButtonContainer";
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

content::WebContents*
HostedAppButtonContainer::GetWebContentsForPageActionIconView() {
  return browser_view_->GetActiveWebContents();
}

BrowserActionsContainer*
HostedAppButtonContainer::GetBrowserActionsContainer() {
  return browser_actions_container_;
}

PageActionIconContainerView*
HostedAppButtonContainer::GetPageActionIconContainerView() {
  return page_action_icon_container_view_;
}

AppMenuButton* HostedAppButtonContainer::GetAppMenuButton() {
  return app_menu_button_;
}

gfx::Rect HostedAppButtonContainer::GetFindBarBoundingBox(
    int contents_height) const {
  if (!IsDrawn())
    return gfx::Rect();

  gfx::Rect anchor_bounds =
      app_menu_button_->ConvertRectToWidget(app_menu_button_->GetLocalBounds());
  if (base::i18n::IsRTL()) {
    // Find bar will be left aligned so align to left edge of app menu button.
    int widget_width = GetWidget()->GetRootView()->width();
    return gfx::Rect(anchor_bounds.x(), anchor_bounds.bottom(),
                     widget_width - anchor_bounds.x(), contents_height);
  }
  // Find bar will be right aligned so align to right edge of app menu button.
  return gfx::Rect(0, anchor_bounds.bottom(),
                   anchor_bounds.x() + anchor_bounds.width(), contents_height);
}

void HostedAppButtonContainer::FocusToolbar() {
  SetPaneFocus(nullptr);
}

views::AccessiblePaneView* HostedAppButtonContainer::GetAsAccessiblePaneView() {
  return this;
}
