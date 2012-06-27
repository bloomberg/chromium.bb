// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_action.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/common/badge_util.h"
#include "googleurl/src/gurl.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"

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


}  // namespace

// Wraps an IconAnimation and implements its ui::AnimationDelegate to erase the
// animation from a map when the animation ends or is cancelled, causing itself
// and its owned IconAnimation to be deleted.
class ExtensionAction::IconAnimationWrapper : public ui::AnimationDelegate {
 public:
  IconAnimationWrapper(ExtensionAction* owner, int tab_id)
      : owner_(owner),
        tab_id_(tab_id),
        ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)) {}

  virtual ~IconAnimationWrapper() {}

  IconAnimation* animation() {
    return &animation_;
  }

 private:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE {
    Done();
  }

  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE {
    Done();
  }

  void Done() {
    owner_->icon_animation_.erase(tab_id_);
    // this will now have been deleted.
  }

  ExtensionAction* owner_;
  int tab_id_;
  IconAnimation animation_;
};

const int ExtensionAction::kDefaultTabId = -1;

ExtensionAction::IconAnimation::IconAnimation(
    ui::AnimationDelegate* delegate)
    // 100ms animation at 50fps (so 5 animation frames in total).
    : ui::LinearAnimation(100, 50, delegate) {}

ExtensionAction::IconAnimation::~IconAnimation() {}

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

void ExtensionAction::IconAnimation::AddObserver(
    ExtensionAction::IconAnimation::Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionAction::IconAnimation::RemoveObserver(
    ExtensionAction::IconAnimation::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionAction::IconAnimation::AnimateToState(double state) {
  FOR_EACH_OBSERVER(Observer, observers_, OnIconChanged(*this));
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
      action_type_(action_type) {
}

ExtensionAction::~ExtensionAction() {
}

scoped_ptr<ExtensionAction> ExtensionAction::CopyForTest() const {
  scoped_ptr<ExtensionAction> copy(
      new ExtensionAction(extension_id_, action_type_));
  copy->popup_url_ = popup_url_;
  copy->title_ = title_;
  copy->icon_ = icon_;
  copy->icon_index_ = icon_index_;
  copy->badge_text_ = badge_text_;
  copy->badge_background_color_ = badge_background_color_;
  copy->badge_text_color_ = badge_text_color_;
  copy->visible_ = visible_;
  copy->icon_animation_ = icon_animation_;
  copy->default_icon_path_ = default_icon_path_;
  copy->id_ = id_;
  copy->icon_paths_ = icon_paths_;
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

void ExtensionAction::SetIcon(int tab_id, const SkBitmap& bitmap) {
  SetValue(&icon_, tab_id, bitmap);
}

SkBitmap ExtensionAction::GetIcon(int tab_id) const {
  return GetValue(&icon_, tab_id);
}

void ExtensionAction::SetIconIndex(int tab_id, int index) {
  if (static_cast<size_t>(index) >= icon_paths_.size()) {
    NOTREACHED();
    return;
  }
  SetValue(&icon_index_, tab_id, index);
}

void ExtensionAction::ClearAllValuesForTab(int tab_id) {
  popup_url_.erase(tab_id);
  title_.erase(tab_id);
  icon_.erase(tab_id);
  icon_index_.erase(tab_id);
  badge_text_.erase(tab_id);
  badge_text_color_.erase(tab_id);
  badge_background_color_.erase(tab_id);
  visible_.erase(tab_id);
  icon_animation_.erase(tab_id);
}

void ExtensionAction::PaintBadge(gfx::Canvas* canvas,
                                 const gfx::Rect& bounds,
                                 int tab_id) {
  std::string text = GetBadgeText(tab_id);
  if (text.empty())
    return;

  SkColor text_color = GetBadgeTextColor(tab_id);
  SkColor background_color = GetBadgeBackgroundColor(tab_id);

  if (SkColorGetA(text_color) == 0x00)
    text_color = SK_ColorWHITE;

  if (SkColorGetA(background_color) == 0x00)
    background_color = SkColorSetARGB(255, 218, 0, 24);  // Default badge color.

  canvas->Save();

  SkPaint* text_paint = badge_util::GetBadgeTextPaintSingleton();
  text_paint->setTextSize(SkFloatToScalar(kTextSize));
  text_paint->setColor(text_color);

  // Calculate text width. We clamp it to a max size.
  SkScalar text_width = text_paint->measureText(text.c_str(), text.size());
  text_width = SkIntToScalar(
      std::min(kMaxTextWidth, SkScalarFloor(text_width)));

  // Calculate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  int badge_width = SkScalarFloor(text_width) + kPadding * 2;
  int icon_width = GetIcon(tab_id).width();
  // Force the pixel width of badge to be either odd (if the icon width is odd)
  // or even otherwise. If there is a mismatch you get http://crbug.com/26400.
  if (icon_width != 0 && (badge_width % 2 != GetIcon(tab_id).width() % 2))
    badge_width += 1;
  badge_width = std::max(kBadgeHeight, badge_width);

  // Paint the badge background color in the right location. It is usually
  // right-aligned, but it can also be center-aligned if it is large.
  SkRect rect;
  rect.fBottom = SkIntToScalar(bounds.bottom() - kBottomMargin);
  rect.fTop = rect.fBottom - SkIntToScalar(kBadgeHeight);
  if (badge_width >= kCenterAlignThreshold) {
    rect.fLeft = SkIntToScalar(
                     SkScalarFloor(SkIntToScalar(bounds.x()) +
                                   SkIntToScalar(bounds.width()) / 2 -
                                   SkIntToScalar(badge_width) / 2));
    rect.fRight = rect.fLeft + SkIntToScalar(badge_width);
  } else {
    rect.fRight = SkIntToScalar(bounds.right());
    rect.fLeft = rect.fRight - badge_width;
  }

  SkPaint rect_paint;
  rect_paint.setStyle(SkPaint::kFill_Style);
  rect_paint.setAntiAlias(true);
  rect_paint.setColor(background_color);
  canvas->sk_canvas()->drawRoundRect(rect, SkIntToScalar(2),
                                     SkIntToScalar(2), rect_paint);

  // Overlay the gradient. It is stretchy, so we do this in three parts.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SkBitmap* gradient_left = rb.GetBitmapNamed(IDR_BROWSER_ACTION_BADGE_LEFT);
  SkBitmap* gradient_right = rb.GetBitmapNamed(IDR_BROWSER_ACTION_BADGE_RIGHT);
  SkBitmap* gradient_center = rb.GetBitmapNamed(
      IDR_BROWSER_ACTION_BADGE_CENTER);

  canvas->sk_canvas()->drawBitmap(*gradient_left, rect.fLeft, rect.fTop);
  canvas->TileImageInt(*gradient_center,
      SkScalarFloor(rect.fLeft) + gradient_left->width(),
      SkScalarFloor(rect.fTop),
      SkScalarFloor(rect.width()) - gradient_left->width() -
                    gradient_right->width(),
      SkScalarFloor(rect.height()));
  canvas->sk_canvas()->drawBitmap(*gradient_right,
      rect.fRight - SkIntToScalar(gradient_right->width()), rect.fTop);

  // Finally, draw the text centered within the badge. We set a clip in case the
  // text was too large.
  rect.fLeft += kPadding;
  rect.fRight -= kPadding;
  canvas->sk_canvas()->clipRect(rect);
  canvas->sk_canvas()->drawText(text.c_str(), text.size(),
                                rect.fLeft + (rect.width() - text_width) / 2,
                                rect.fTop + kTextSize + kTopTextPadding,
                                *text_paint);
  canvas->Restore();
}

base::WeakPtr<ExtensionAction::IconAnimation> ExtensionAction::GetIconAnimation(
    int tab_id) const {
  std::map<int, linked_ptr<IconAnimationWrapper> >::const_iterator it =
      icon_animation_.find(tab_id);
  return (it != icon_animation_.end()) ? it->second->animation()->AsWeakPtr()
                                       : base::WeakPtr<IconAnimation>();
}

void ExtensionAction::RunIconAnimation(int tab_id) {
  IconAnimationWrapper* icon_animation =
      new IconAnimationWrapper(this, tab_id);
  icon_animation_[tab_id] = make_linked_ptr(icon_animation);
  icon_animation->animation()->Start();
}
