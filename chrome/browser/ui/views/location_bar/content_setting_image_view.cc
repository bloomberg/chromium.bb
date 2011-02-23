// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/content_setting_image_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/skia_util.h"
#include "views/border.h"

namespace {
// Animation parameters.
const int kOpenTimeMs = 150;
const int kFullOpenedTimeMs = 3200;
const int kMoveTimeMs = kFullOpenedTimeMs + 2 * kOpenTimeMs;
const int kFrameRateHz = 60;
// Colors for the animated box.
const SkColor kTopBoxColor = SkColorSetRGB(0xff, 0xf8, 0xd4);
const SkColor kBottomBoxColor = SkColorSetRGB(0xff, 0xe6, 0xaf);
const SkColor kBorderColor = SkColorSetRGB(0xe9, 0xb9, 0x66);
// Corner radius of the animated box.
const SkScalar kBoxCornerRadius = 2;
// Margins for animated box.
const int kTextMarginPixels = 4;
const int kIconLeftMargin = 4;


// The fraction of the animation we'll spend animating the string into view, and
// then again animating it closed -  total animation (slide out, show, then
// slide in) is 1.0.
const double kAnimatingFraction = kOpenTimeMs * 1.0 / kMoveTimeMs;
}

ContentSettingImageView::ContentSettingImageView(
    ContentSettingsType content_type,
    LocationBarView* parent,
    Profile* profile)
    : ui::LinearAnimation(kMoveTimeMs, kFrameRateHz, NULL),
      content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              content_type)),
      parent_(parent),
      profile_(profile),
      info_bubble_(NULL),
      animation_in_progress_(false),
      text_size_(0),
      visible_text_size_(0) {
  SetHorizontalAlignment(ImageView::LEADING);
}

ContentSettingImageView::~ContentSettingImageView() {
  if (info_bubble_)
    info_bubble_->Close();
}

void ContentSettingImageView::UpdateFromTabContents(TabContents* tab_contents) {
  content_setting_image_model_->UpdateFromTabContents(tab_contents);
  if (!content_setting_image_model_->is_visible()) {
    SetVisible(false);
    return;
  }
  SetImage(ResourceBundle::GetSharedInstance().GetBitmapNamed(
      content_setting_image_model_->get_icon()));
  SetTooltipText(UTF8ToWide(content_setting_image_model_->get_tooltip()));
  SetVisible(true);

  TabSpecificContentSettings* content_settings = tab_contents ?
      tab_contents->GetTabSpecificContentSettings() : NULL;
  if (!content_settings || content_settings->IsBlockageIndicated(
      content_setting_image_model_->get_content_settings_type()))
    return;

  // The content blockage was not yet indicated to the user. Start indication
  // animation and clear "not yet shown" flag.
  content_settings->SetBlockageHasBeenIndicated(
      content_setting_image_model_->get_content_settings_type());

  int animated_string_id =
      content_setting_image_model_->explanatory_string_id();
  // Check if the animation is enabled and if the string for animation is
  // available.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableBlockContentAnimation) || !animated_string_id)
    return;

  // Do not start animation if already in progress.
  if (!animation_in_progress_) {
    animation_in_progress_ = true;
    // Initialize animated string. It will be cleared when animation is
    // completed.
    animated_text_ = l10n_util::GetStringUTF16(animated_string_id);
    text_size_ = ResourceBundle::GetSharedInstance().GetFont(
        ResourceBundle::MediumFont).GetStringWidth(animated_text_);
    text_size_ += 2 * kTextMarginPixels + kIconLeftMargin;
    if (border())
      border()->GetInsets(&saved_insets_);
    Start();
  }
}

gfx::Size ContentSettingImageView::GetPreferredSize() {
  gfx::Size preferred_size(views::ImageView::GetPreferredSize());
  // When view is animated visible_text_size_ > 0, it is 0 otherwise.
  preferred_size.set_width(preferred_size.width() + visible_text_size_);
  return preferred_size;
}

bool ContentSettingImageView::OnMousePressed(const views::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void ContentSettingImageView::OnMouseReleased(const views::MouseEvent& event,
                                              bool canceled) {
  if (canceled || !HitTest(event.location()))
    return;

  TabContents* tab_contents = parent_->GetTabContentsWrapper()->tab_contents();
  if (!tab_contents)
    return;

  // Prerender does not have a bubble.
  ContentSettingsType content_settings_type =
      content_setting_image_model_->get_content_settings_type();
  if (content_settings_type == CONTENT_SETTINGS_TYPE_PRERENDER)
    return;

  gfx::Rect screen_bounds(GetImageBounds());
  gfx::Point origin(screen_bounds.origin());
  views::View::ConvertPointToScreen(this, &origin);
  screen_bounds.set_origin(origin);
  ContentSettingBubbleContents* bubble_contents =
      new ContentSettingBubbleContents(
          ContentSettingBubbleModel::CreateContentSettingBubbleModel(
              tab_contents, profile_, content_settings_type),
          profile_, tab_contents);
  info_bubble_ = InfoBubble::Show(GetWidget(), screen_bounds,
      BubbleBorder::TOP_RIGHT, bubble_contents, this);
  bubble_contents->set_info_bubble(info_bubble_);
}

void ContentSettingImageView::VisibilityChanged(View* starting_from,
                                                bool is_visible) {
  if (!is_visible && info_bubble_)
    info_bubble_->Close();
}

void ContentSettingImageView::OnPaint(gfx::Canvas* canvas) {
  gfx::Insets current_insets;
  if (border())
    border()->GetInsets(&current_insets);
  // During the animation we draw a border, an icon and the text. The text area
  // is changing in size during the animation, giving the appearance of the text
  // sliding out and then back in. When the text completely slid out the yellow
  // border is no longer painted around the icon. |visible_text_size_| is 0 when
  // animation is stopped.
  int necessary_left_margin = std::min(kIconLeftMargin, visible_text_size_);
  if (necessary_left_margin != current_insets.left() - saved_insets_.left()) {
    // In the non-animated state borders' left() is 0, in the animated state it
    // is the kIconLeftMargin, so we need to animate border reduction when it
    // starts to disappear.
    views::Border* empty_border = views::Border::CreateEmptyBorder(
        saved_insets_.top(),
        saved_insets_.left() + necessary_left_margin,
        saved_insets_.bottom(),
        saved_insets_.right());
    set_border(empty_border);
  }
  // Paint an icon with possibly non-empty left border.
  views::ImageView::OnPaint(canvas);
  if (animation_in_progress_) {
    // Paint text to the right of the icon.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    canvas->DrawStringInt(animated_text_,
        rb.GetFont(ResourceBundle::MediumFont), SK_ColorBLACK,
        GetImageBounds().right() + kTextMarginPixels, y(),
        width() - GetImageBounds().width(), height(),
        gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::TEXT_VALIGN_MIDDLE);
  }
}

void ContentSettingImageView::OnPaintBackground(gfx::Canvas* canvas) {
  if (!animation_in_progress_) {
    views::ImageView::OnPaintBackground(canvas);
    return;
  }
  // Paint yellow gradient background if in animation mode.
  const int kEdgeThickness = 1;
  SkPaint paint;
  paint.setShader(gfx::CreateGradientShader(kEdgeThickness,
                  height() - (2 * kEdgeThickness),
                  kTopBoxColor, kBottomBoxColor));
  SkSafeUnref(paint.getShader());
  SkRect color_rect;
  color_rect.iset(0, 0, width() - 1, height() - 1);
  canvas->AsCanvasSkia()->drawRoundRect(color_rect, kBoxCornerRadius,
                                        kBoxCornerRadius, paint);
  SkPaint outer_paint;
  outer_paint.setStyle(SkPaint::kStroke_Style);
  outer_paint.setColor(kBorderColor);
  color_rect.inset(SkIntToScalar(kEdgeThickness),
                   SkIntToScalar(kEdgeThickness));
  canvas->AsCanvasSkia()->drawRoundRect(color_rect, kBoxCornerRadius,
                                        kBoxCornerRadius, outer_paint);
}

void ContentSettingImageView::InfoBubbleClosing(InfoBubble* info_bubble,
                                                bool closed_by_escape) {
  info_bubble_ = NULL;
}

bool ContentSettingImageView::CloseOnEscape() {
  return true;
}

void ContentSettingImageView::AnimateToState(double state) {
  if (state >= 1.0) {
    // Animaton is over, clear the variables.
    animation_in_progress_ = false;
    visible_text_size_ = 0;
  } else if (state < kAnimatingFraction) {
    visible_text_size_ = static_cast<int>(text_size_ * state /
                                          kAnimatingFraction);
  } else if (state > (1.0 - kAnimatingFraction)) {
    visible_text_size_ = static_cast<int>(text_size_ * (1.0 - state) /
                                          kAnimatingFraction);
  } else {
    visible_text_size_ = text_size_;
  }
  parent_->Layout();
  parent_->SchedulePaint();
}

