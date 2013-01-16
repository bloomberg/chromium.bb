// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/badge_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/icon_with_badge_image_source.h"
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
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {

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

ExtensionAction::ExtensionAction(
    const std::string& extension_id,
    extensions::ActionInfo::Type action_type,
    const extensions::ActionInfo& manifest_data)
    : extension_id_(extension_id),
      action_type_(action_type),
      has_changed_(false) {
  // Page/script actions are hidden/disabled by default, and browser actions are
  // visible/enabled by default.
  SetAppearance(kDefaultTabId,
                action_type == extensions::ActionInfo::TYPE_BROWSER ?
                ExtensionAction::ACTIVE : ExtensionAction::INVISIBLE);
  SetTitle(kDefaultTabId, manifest_data.default_title);
  SetPopupUrl(kDefaultTabId, manifest_data.default_popup_url);
  if (!manifest_data.default_icon.empty()) {
    set_default_icon(make_scoped_ptr(new ExtensionIconSet(
        manifest_data.default_icon)));
  }
  set_id(manifest_data.id);
}

ExtensionAction::~ExtensionAction() {
}

scoped_ptr<ExtensionAction> ExtensionAction::CopyForTest() const {
  scoped_ptr<ExtensionAction> copy(
      new ExtensionAction(extension_id_, action_type_,
                          extensions::ActionInfo()));
  copy->popup_url_ = popup_url_;
  copy->title_ = title_;
  copy->icon_ = icon_;
  copy->badge_text_ = badge_text_;
  copy->badge_background_color_ = badge_background_color_;
  copy->badge_text_color_ = badge_text_color_;
  copy->appearance_ = appearance_;
  copy->icon_animation_ = icon_animation_;
  copy->id_ = id_;

  if (default_icon_.get())
    copy->default_icon_.reset(new ExtensionIconSet(*default_icon_));

  return copy.Pass();
}

// static
int ExtensionAction::GetIconSizeForType(
    extensions::ActionInfo::Type type) {
  switch (type) {
    case extensions::ActionInfo::TYPE_BROWSER:
    case extensions::ActionInfo::TYPE_PAGE:
    case extensions::ActionInfo::TYPE_SYSTEM_INDICATOR:
      // TODO(dewittj) Report the actual icon size of the system
      // indicator.
      return extension_misc::EXTENSION_ICON_ACTION;
    case extensions::ActionInfo::TYPE_SCRIPT_BADGE:
      return extension_misc::EXTENSION_ICON_BITTY;
    default:
      NOTREACHED();
      return 0;
  }
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

void ExtensionAction::SetIcon(int tab_id, const gfx::Image& image) {
  SetValue(&icon_, tab_id, image.AsImageSkia());
}

gfx::Image ExtensionAction::ApplyAttentionAndAnimation(
    const gfx::ImageSkia& original_icon,
    int tab_id) const {
  gfx::ImageSkia icon = original_icon;
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

  // When showing a script badge for the first time on a web page, fade it in.
  // Other transitions happen instantly.
  if (old_appearance == INVISIBLE && tab_id != kDefaultTabId &&
      action_type_ == extensions::ActionInfo::TYPE_SCRIPT_BADGE) {
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
  badge_util::PaintBadge(
      canvas,
      bounds,
      GetBadgeText(tab_id),
      GetBadgeTextColor(tab_id),
      GetBadgeBackgroundColor(tab_id),
      GetIconWidth(tab_id),
      action_type());
}

gfx::ImageSkia ExtensionAction::GetIconWithBadge(
    const gfx::ImageSkia& icon,
    int tab_id,
    const gfx::Size& spacing) const {
  if (tab_id < 0)
    return icon;

  return gfx::ImageSkia(
      new IconWithBadgeImageSource(icon,
                                   icon.size(),
                                   spacing,
                                   GetBadgeText(tab_id),
                                   GetBadgeTextColor(tab_id),
                                   GetBadgeBackgroundColor(tab_id),
                                   action_type()),
     icon.size());
}

// Determines which icon would be returned by |GetIcon|, and returns its width.
int ExtensionAction::GetIconWidth(int tab_id) const {
  // If icon has been set, return its width.
  gfx::ImageSkia icon = GetValue(&icon_, tab_id);
  if (!icon.isNull())
    return icon.width();
  // If there is a default icon, the icon width will be set depending on our
  // action type.
  if (default_icon_.get())
    return GetIconSizeForType(action_type());

  // If no icon has been set and there is no default icon, we need favicon
  // width.
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_EXTENSIONS_FAVICON).ToImageSkia()->width();
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
