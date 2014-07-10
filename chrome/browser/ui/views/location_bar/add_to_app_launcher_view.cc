// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/add_to_app_launcher_view.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace {

// TODO(benwells): Refactor common code between this and
// ContentSettingImageView.
const int kBackgroundImages[] = IMAGE_GRID(IDR_OMNIBOX_CONTENT_SETTING_BUBBLE);
const int kStayOpenTimeMS = 3200;  // Time spent with animation fully open.

const int kOpenTimeMS = 150;
const int kAnimationDurationMS = (kOpenTimeMS * 2) + kStayOpenTimeMS;

// Amount of padding at the edges of the bubble.  If |by_icon| is true, this
// is the padding next to the icon; otherwise it's the padding next to the
// label.  (We increase padding next to the label by the amount of padding
// "built in" to the icon in order to make the bubble appear to have
// symmetrical padding.)
int GetBubbleOuterPadding(bool by_icon) {
  return LocationBarView::kItemPadding - LocationBarView::kBubblePadding +
      (by_icon ? 0 : LocationBarView::kIconInternalPadding);
}

int GetTotalSpacingWhileAnimating() {
  return GetBubbleOuterPadding(true) + LocationBarView::kItemPadding +
      GetBubbleOuterPadding(false);
}

}  // namespace

AddToAppLauncherView::AddToAppLauncherView(LocationBarView* parent,
                                           const gfx::FontList& font_list,
                                           SkColor text_color,
                                           SkColor parent_background_color)
    : parent_(parent),
      background_painter_(
          views::Painter::CreateImageGridPainter(kBackgroundImages)),
      icon_(new views::ImageView),
      text_label_(new views::Label(base::string16(), font_list)),
      slide_animator_(this) {
  icon_->SetHorizontalAlignment(views::ImageView::LEADING);
  icon_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_BOOKMARK_BAR_APPS_SHORTCUT));
  icon_->SetTooltipText(
      base::UTF8ToUTF16(l10n_util::GetStringUTF8(IDS_ADD_TO_APP_LIST_HINT)));
  AddChildView(icon_);

  text_label_->SetVisible(false);
  text_label_->SetEnabledColor(text_color);
  // Calculate the actual background color for the label.  The background images
  // are painted atop |parent_background_color|.  We grab the color of the
  // middle pixel of the middle image of the background, which we treat as the
  // representative color of the entire background (reasonable, given the
  // current appearance of these images).  Then we alpha-blend it over the
  // parent background color to determine the actual color the label text will
  // sit atop.
  const SkBitmap& bitmap(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          kBackgroundImages[4])->GetRepresentation(1.0f).sk_bitmap());
  SkAutoLockPixels pixel_lock(bitmap);
  SkColor background_image_color =
      bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2);
  // Tricky bit: We alpha blend an opaque version of |background_image_color|
  // against |parent_background_color| using the original image grid color's
  // alpha. This is because AlphaBlend(a, b, 255) always returns |a| unchanged
  // even if |a| is a color with non-255 alpha.
  text_label_->SetBackgroundColor(
      color_utils::AlphaBlend(SkColorSetA(background_image_color, 255),
                              parent_background_color,
                              SkColorGetA(background_image_color)));
  text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label_->SetElideBehavior(gfx::NO_ELIDE);
  text_label_->SetText(base::UTF8ToUTF16(
      l10n_util::GetStringUTF8(IDS_ADD_TO_APP_LIST_NOTIFICATION_TEXT)));
  AddChildView(text_label_);

  slide_animator_.SetSlideDuration(kAnimationDurationMS);
  slide_animator_.SetTweenType(gfx::Tween::LINEAR);
}

AddToAppLauncherView::~AddToAppLauncherView() {
}

void AddToAppLauncherView::Update(content::WebContents* web_contents) {
  SetVisible(false);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableProminentURLAppFlow) ||
      !command_line->HasSwitch(switches::kEnableStreamlinedHostedApps) ||
      !web_contents || web_contents->IsLoading() ||
      !web_contents->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    slide_animator_.Reset();
    return;
  }

  SetVisible(true);

  if (!slide_animator_.is_animating()) {
    text_label_->SetVisible(true);
    slide_animator_.Show();
  }
}

void AddToAppLauncherView::AnimationEnded(const gfx::Animation* animation) {
  slide_animator_.Reset();
  text_label_->SetVisible(false);
  AnimationProgressed(animation);
}

void AddToAppLauncherView::AnimationProgressed(
    const gfx::Animation* animation) {
  parent_->Layout();
  parent_->SchedulePaint();
}

void AddToAppLauncherView::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

gfx::Size AddToAppLauncherView::GetPreferredSize() const {
  // Height will be ignored by the LocationBarView.
  gfx::Size size(icon_->GetPreferredSize());
  if (slide_animator_.is_animating()) {
    double state = slide_animator_.GetCurrentValue();
    // The fraction of the animation we'll spend animating the string into view,
    // which is also the fraction we'll spend animating it closed; total
    // animation (slide out, show, then slide in) is 1.0.
    const double kOpenFraction =
        static_cast<double>(kOpenTimeMS) / kAnimationDurationMS;
    double size_fraction = 1.0;
    if (state < kOpenFraction)
      size_fraction = state / kOpenFraction;
    if (state > (1.0 - kOpenFraction))
      size_fraction = (1.0 - state) / kOpenFraction;
    size.Enlarge(
        size_fraction * (text_label_->GetPreferredSize().width() +
            GetTotalSpacingWhileAnimating()), 0);
    size.SetToMax(background_painter_->GetMinimumSize());
  }
  return size;
}

void AddToAppLauncherView::Layout() {
  const int icon_width = icon_->GetPreferredSize().width();
  icon_->SetBounds(
      std::min((width() - icon_width) / 2, GetBubbleOuterPadding(true)),
      0,
      icon_width,
      height());
  text_label_->SetBounds(
      icon_->bounds().right() + LocationBarView::kItemPadding,
      0,
      std::max(width() - GetTotalSpacingWhileAnimating() - icon_width, 0),
      height());
}

void AddToAppLauncherView::OnPaintBackground(gfx::Canvas* canvas) {
  if (slide_animator_.is_animating())
    background_painter_->Paint(canvas, size());
}
