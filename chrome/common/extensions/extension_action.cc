// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_action.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/badge_util.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {

// Different platforms need slightly different constants to look good.
#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
const float kTextSize = 9.0;
const int kBottomMargin = 0;
const int kPadding = 2;
const int kTopTextPadding = 0;
#elif defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
const float kTextSize = 8.0;
const int kBottomMargin = 5;
const int kPadding = 2;
const int kTopTextPadding = 1;
#elif defined(OS_MACOSX)
const float kTextSize = 9.0;
const int kBottomMargin = 5;
const int kPadding = 2;
const int kTopTextPadding = 0;
#else
const float kTextSize = 10;
const int kBottomMargin = 5;
const int kPadding = 2;
// The padding between the top of the badge and the top of the text.
const int kTopTextPadding = -1;
#endif

const int kBadgeHeight = 11;
const int kMaxTextWidth = 23;
// The minimum width for center-aligning the badge.
const int kCenterAlignThreshold = 20;

class GetAttentionImageSource : public gfx::ImageSkiaSource {
 public:
  explicit GetAttentionImageSource(const gfx::ImageSkia& icon)
      : icon_(icon) {}

  // gfx::ImageSkiaSource overrides:
  virtual gfx::ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor)
      OVERRIDE {
    gfx::ImageSkiaRep icon_rep = icon_.GetRepresentation(scale_factor);
    color_utils::HSL shift = {-1, 0, 0.5};
    return gfx::ImageSkiaRep(
        SkBitmapOperations::CreateHSLShiftedBitmap(icon_rep.sk_bitmap(), shift),
        icon_rep.scale_factor());
  }

 private:
  const gfx::ImageSkia icon_;
};

}  // namespace

// TODO(tbarzic): Merge AnimationIconImageSource and IconAnimation together.
// Source for painting animated skia image.
class AnimatedIconImageSource : public gfx::ImageSkiaSource {
 public:
  AnimatedIconImageSource(
      const gfx::ImageSkia& image,
      base::WeakPtr<ExtensionAction::IconAnimation> animation)
      : image_(image),
        animation_(animation) {
  }

 private:
  virtual ~AnimatedIconImageSource() {}

  virtual gfx::ImageSkiaRep GetImageForScale(ui::ScaleFactor scale) OVERRIDE {
    gfx::ImageSkiaRep original_rep = image_.GetRepresentation(scale);
    if (!animation_)
      return original_rep;

    // Original representation's scale factor may be different from scale
    // factor passed to this method. We want to use the former (since we are
    // using bitmap for that scale).
    return gfx::ImageSkiaRep(
        animation_->Apply(original_rep.sk_bitmap()),
        original_rep.scale_factor());
  }

  gfx::ImageSkia image_;
  base::WeakPtr<ExtensionAction::IconAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(AnimatedIconImageSource);
};

// CanvasImageSource for creating browser action icon with a badge.
class ExtensionAction::IconWithBadgeImageSource
    : public gfx::CanvasImageSource {
 public:
  IconWithBadgeImageSource(const gfx::ImageSkia& icon,
                           const gfx::Size& spacing,
                           const std::string& text,
                           const SkColor& text_color,
                           const SkColor& background_color)
      : gfx::CanvasImageSource(icon.size(), false),
        icon_(icon),
        spacing_(spacing),
        text_(text),
        text_color_(text_color),
        background_color_(background_color) {
  }

  virtual ~IconWithBadgeImageSource() {}

 private:
  virtual void Draw(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawImageInt(icon_, 0, 0, SkPaint());

    gfx::Rect bounds(size_.width() + spacing_.width(),
                     size_.height() + spacing_.height());

    // Draw a badge on the provided browser action icon's canvas.
    ExtensionAction::DoPaintBadge(canvas, bounds, text_, text_color_,
        background_color_, size_.width());
  }

  // Browser action icon image.
  gfx::ImageSkia icon_;
  // Extra spacing for badge compared to icon bounds.
  gfx::Size spacing_;
  // Text to be displayed on the badge.
  std::string text_;
  // Color of badge text.
  SkColor text_color_;
  // Color of the badge.
  SkColor background_color_;

  DISALLOW_COPY_AND_ASSIGN(IconWithBadgeImageSource);
};


const int ExtensionAction::kDefaultTabId = -1;
// 100ms animation at 50fps (so 5 animation frames in total).
const int kIconFadeInDurationMs = 100;
const int kIconFadeInFramesPerSecond = 50;

ExtensionAction::IconAnimation::IconAnimation()
    : ui::LinearAnimation(kIconFadeInDurationMs, kIconFadeInFramesPerSecond,
                          NULL),
      weak_ptr_factory_(this) {}

ExtensionAction::IconAnimation::~IconAnimation() {
  // Make sure observers don't access *this after its destructor has started.
  weak_ptr_factory_.InvalidateWeakPtrs();
  // In case the animation was destroyed before it finished (likely due to
  // delays in timer scheduling), make sure it's fully visible.
  FOR_EACH_OBSERVER(Observer, observers_, OnIconChanged());
}

const SkBitmap& ExtensionAction::IconAnimation::Apply(
    const SkBitmap& icon) const {
  DCHECK_GT(icon.width(), 0);
  DCHECK_GT(icon.height(), 0);

  if (!device_.get() ||
      (device_->width() != icon.width()) ||
      (device_->height() != icon.height())) {
    device_.reset(new SkDevice(
      SkBitmap::kARGB_8888_Config, icon.width(), icon.height(), true));
  }

  SkCanvas canvas(device_.get());
  canvas.clear(SK_ColorWHITE);
  SkPaint paint;
  paint.setAlpha(CurrentValueBetween(0, 255));
  canvas.drawBitmap(icon, 0, 0, &paint);
  return device_->accessBitmap(false);
}

base::WeakPtr<ExtensionAction::IconAnimation>
ExtensionAction::IconAnimation::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ExtensionAction::IconAnimation::AddObserver(
    ExtensionAction::IconAnimation::Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionAction::IconAnimation::RemoveObserver(
    ExtensionAction::IconAnimation::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionAction::IconAnimation::AnimateToState(double state) {
  FOR_EACH_OBSERVER(Observer, observers_, OnIconChanged());
}

ExtensionAction::IconAnimation::ScopedObserver::ScopedObserver(
    const base::WeakPtr<IconAnimation>& icon_animation,
    Observer* observer)
    : icon_animation_(icon_animation),
      observer_(observer) {
  if (icon_animation.get())
    icon_animation->AddObserver(observer);
}

ExtensionAction::IconAnimation::ScopedObserver::~ScopedObserver() {
  if (icon_animation_.get())
    icon_animation_->RemoveObserver(observer_);
}

ExtensionAction::ExtensionAction(const std::string& extension_id,
                                 Type action_type)
    : extension_id_(extension_id),
      action_type_(action_type),
      has_changed_(false) {
}

ExtensionAction::~ExtensionAction() {
}

scoped_ptr<ExtensionAction> ExtensionAction::CopyForTest() const {
  scoped_ptr<ExtensionAction> copy(
      new ExtensionAction(extension_id_, action_type_));
  copy->popup_url_ = popup_url_;
  copy->title_ = title_;
  copy->icon_ = icon_;
  copy->badge_text_ = badge_text_;
  copy->badge_background_color_ = badge_background_color_;
  copy->badge_text_color_ = badge_text_color_;
  copy->appearance_ = appearance_;
  copy->icon_animation_ = icon_animation_;
  copy->default_icon_path_ = default_icon_path_;
  copy->id_ = id_;
  return copy.Pass();
}

void ExtensionAction::SetPopupUrl(int tab_id, const GURL& url) {
  // We store |url| even if it is empty, rather than removing a URL from the
  // map.  If an extension has a default popup, and removes it for a tab via
  // the API, we must remember that there is no popup for that specific tab.
  // If we removed the tab's URL, GetPopupURL would incorrectly return the
  // default URL.
  SetValue(&popup_url_, tab_id, url);
}

bool ExtensionAction::HasPopup(int tab_id) const {
  return !GetPopupUrl(tab_id).is_empty();
}

GURL ExtensionAction::GetPopupUrl(int tab_id) const {
  return GetValue(&popup_url_, tab_id);
}

void ExtensionAction::CacheIcon(const gfx::Image& icon) {
  if (!icon.IsEmpty())
    cached_icon_.reset(new gfx::ImageSkia(*icon.ToImageSkia()));
}

void ExtensionAction::SetIcon(int tab_id, const gfx::Image& image) {
  SetValue(&icon_, tab_id, image.AsImageSkia());
}

gfx::Image ExtensionAction::GetIcon(int tab_id) const {
  // Check if a specific icon is set for this tab.
  gfx::ImageSkia icon = GetExplicitlySetIcon(tab_id);
  if (icon.isNull()) {
    if (cached_icon_.get()) {
      icon = *cached_icon_;
    } else {
      icon = *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_EXTENSIONS_FAVICON);
    }
  }

  if (GetValue(&appearance_, tab_id) == WANTS_ATTENTION)
    icon = gfx::ImageSkia(new GetAttentionImageSource(icon), icon.size());

  return gfx::Image(ApplyIconAnimation(tab_id, icon));
}

gfx::ImageSkia ExtensionAction::GetExplicitlySetIcon(int tab_id) const {
  return GetValue(&icon_, tab_id);
}

bool ExtensionAction::SetAppearance(int tab_id, Appearance new_appearance) {
  const Appearance old_appearance = GetValue(&appearance_, tab_id);

  if (old_appearance == new_appearance)
    return false;

  SetValue(&appearance_, tab_id, new_appearance);

  // When showing a badge for the first time on a web page, fade it
  // in.  Other transitions happen instantly.
  if (old_appearance == INVISIBLE && tab_id != kDefaultTabId) {
    RunIconAnimation(tab_id);
  }

  return true;
}

void ExtensionAction::ClearAllValuesForTab(int tab_id) {
  popup_url_.erase(tab_id);
  title_.erase(tab_id);
  icon_.erase(tab_id);
  badge_text_.erase(tab_id);
  badge_text_color_.erase(tab_id);
  badge_background_color_.erase(tab_id);
  appearance_.erase(tab_id);
  icon_animation_.erase(tab_id);
}

void ExtensionAction::PaintBadge(gfx::Canvas* canvas,
                                 const gfx::Rect& bounds,
                                 int tab_id) {
  ExtensionAction::DoPaintBadge(
      canvas,
      bounds,
      GetBadgeText(tab_id),
      GetBadgeTextColor(tab_id),
      GetBadgeBackgroundColor(tab_id),
      GetValue(&icon_, tab_id).size().width());
}

gfx::ImageSkia ExtensionAction::GetIconWithBadge(
    const gfx::ImageSkia& icon,
    int tab_id,
    const gfx::Size& spacing) const {
  if (tab_id < 0)
    return icon;

  return gfx::ImageSkia(
      new IconWithBadgeImageSource(icon,
                                   spacing,
                                   GetBadgeText(tab_id),
                                   GetBadgeTextColor(tab_id),
                                   GetBadgeBackgroundColor(tab_id)),
     icon.size());
}

// static
void ExtensionAction::DoPaintBadge(gfx::Canvas* canvas,
                                   const gfx::Rect& bounds,
                                   const std::string& text,
                                   const SkColor& text_color_in,
                                   const SkColor& background_color_in,
                                   int icon_width) {
  if (text.empty())
    return;

  SkColor text_color = text_color_in;
  if (SkColorGetA(text_color_in) == 0x00)
    text_color = SK_ColorWHITE;

  SkColor background_color = background_color_in;
  if (SkColorGetA(background_color_in) == 0x00)
      background_color = SkColorSetARGB(255, 218, 0, 24);

  canvas->Save();

  SkPaint* text_paint = badge_util::GetBadgeTextPaintSingleton();
  text_paint->setTextSize(SkFloatToScalar(kTextSize));
  text_paint->setColor(text_color);

  // Calculate text width. We clamp it to a max size.
  SkScalar sk_text_width = text_paint->measureText(text.c_str(), text.size());
  int text_width = std::min(kMaxTextWidth, SkScalarFloor(sk_text_width));

  // Calculate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  int badge_width = text_width + kPadding * 2;
  // Force the pixel width of badge to be either odd (if the icon width is odd)
  // or even otherwise. If there is a mismatch you get http://crbug.com/26400.
  if (icon_width != 0 && (badge_width % 2 != icon_width % 2))
    badge_width += 1;
  badge_width = std::max(kBadgeHeight, badge_width);

  // Paint the badge background color in the right location. It is usually
  // right-aligned, but it can also be center-aligned if it is large.
  int rect_height = kBadgeHeight;
  int rect_y = bounds.bottom() - kBottomMargin - kBadgeHeight;
  int rect_width = badge_width;
  int rect_x = (badge_width >= kCenterAlignThreshold) ?
      (bounds.x() + bounds.width() - badge_width) / 2 :
      bounds.right() - badge_width;
  gfx::Rect rect(rect_x, rect_y, rect_width, rect_height);

  SkPaint rect_paint;
  rect_paint.setStyle(SkPaint::kFill_Style);
  rect_paint.setAntiAlias(true);
  rect_paint.setColor(background_color);
  canvas->DrawRoundRect(rect, 2, rect_paint);

  // Overlay the gradient. It is stretchy, so we do this in three parts.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* gradient_left = rb.GetImageSkiaNamed(
      IDR_BROWSER_ACTION_BADGE_LEFT);
  gfx::ImageSkia* gradient_right = rb.GetImageSkiaNamed(
      IDR_BROWSER_ACTION_BADGE_RIGHT);
  gfx::ImageSkia* gradient_center = rb.GetImageSkiaNamed(
      IDR_BROWSER_ACTION_BADGE_CENTER);

  canvas->DrawImageInt(*gradient_left, rect.x(), rect.y());
  canvas->TileImageInt(*gradient_center,
      rect.x() + gradient_left->width(),
      rect.y(),
      rect.width() - gradient_left->width() - gradient_right->width(),
      rect.height());
  canvas->DrawImageInt(*gradient_right,
      rect.right() - gradient_right->width(), rect.y());

  // Finally, draw the text centered within the badge. We set a clip in case the
  // text was too large.
  rect.Inset(kPadding, 0);
  canvas->ClipRect(rect);
  canvas->sk_canvas()->drawText(
      text.c_str(), text.size(),
      SkFloatToScalar(rect.x() +
                      static_cast<float>(rect.width() - text_width) / 2),
      SkFloatToScalar(rect.y() + kTextSize + kTopTextPadding),
      *text_paint);
  canvas->Restore();
}

base::WeakPtr<ExtensionAction::IconAnimation> ExtensionAction::GetIconAnimation(
    int tab_id) const {
  std::map<int, base::WeakPtr<IconAnimation> >::iterator it =
      icon_animation_.find(tab_id);
  if (it == icon_animation_.end())
    return base::WeakPtr<ExtensionAction::IconAnimation>();
  if (it->second)
    return it->second;

  // Take this opportunity to remove all the NULL IconAnimations from
  // icon_animation_.
  icon_animation_.erase(it);
  for (it = icon_animation_.begin(); it != icon_animation_.end();) {
    if (it->second) {
      ++it;
    } else {
      // The WeakPtr is null; remove it from the map.
      icon_animation_.erase(it++);
    }
  }
  return base::WeakPtr<ExtensionAction::IconAnimation>();
}

gfx::ImageSkia ExtensionAction::ApplyIconAnimation(
    int tab_id,
    const gfx::ImageSkia& icon) const {
  base::WeakPtr<IconAnimation> animation = GetIconAnimation(tab_id);
  if (animation == NULL)
    return icon;

  return gfx::ImageSkia(new AnimatedIconImageSource(icon, animation),
                        icon.size());
}

namespace {
// Used to create a Callback owning an IconAnimation.
void DestroyIconAnimation(scoped_ptr<ExtensionAction::IconAnimation>) {}
}
void ExtensionAction::RunIconAnimation(int tab_id) {
  scoped_ptr<IconAnimation> icon_animation(new IconAnimation());
  icon_animation_[tab_id] = icon_animation->AsWeakPtr();
  icon_animation->Start();
  // After the icon is finished fading in (plus some padding to handle random
  // timer delays), destroy it. We use a delayed task so that the Animation is
  // deleted even if it hasn't finished by the time the MessageLoop is
  // destroyed.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DestroyIconAnimation, base::Passed(icon_animation.Pass())),
      base::TimeDelta::FromMilliseconds(kIconFadeInDurationMs * 2));
}
