// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/common/badge_util.h"
#include "chrome/common/icon_with_badge_image_source.h"
#include "extensions/common/constants.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace {

class GetAttentionImageSource : public gfx::ImageSkiaSource {
 public:
  explicit GetAttentionImageSource(const gfx::ImageSkia& icon)
      : icon_(icon) {}

  // gfx::ImageSkiaSource overrides:
  virtual gfx::ImageSkiaRep GetImageForScale(float scale) OVERRIDE {
    gfx::ImageSkiaRep icon_rep = icon_.GetRepresentation(scale);
    color_utils::HSL shift = {-1, 0, 0.5};
    return gfx::ImageSkiaRep(
        SkBitmapOperations::CreateHSLShiftedBitmap(icon_rep.sk_bitmap(), shift),
        icon_rep.scale());
  }

 private:
  const gfx::ImageSkia icon_;
};

template <class T>
bool HasValue(const std::map<int, T>& map, int tab_id) {
  return map.find(tab_id) != map.end();
}

}  // namespace

const int ExtensionAction::kDefaultTabId = -1;
const int ExtensionAction::kPageActionIconMaxSize = 19;

ExtensionAction::ExtensionAction(const std::string& extension_id,
                                 extensions::ActionInfo::Type action_type,
                                 const extensions::ActionInfo& manifest_data)
    : extension_id_(extension_id), action_type_(action_type) {
  // Page/script actions are hidden/disabled by default, and browser actions are
  // visible/enabled by default.
  SetIsVisible(kDefaultTabId,
               action_type == extensions::ActionInfo::TYPE_BROWSER);
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
  copy->is_visible_ = is_visible_;
  copy->id_ = id_;

  if (default_icon_)
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

gfx::ImageSkia ExtensionAction::GetExplicitlySetIcon(int tab_id) const {
  return GetValue(&icon_, tab_id);
}

bool ExtensionAction::SetIsVisible(int tab_id, bool new_visibility) {
  const bool old_visibility = GetValue(&is_visible_, tab_id);

  if (old_visibility == new_visibility)
    return false;

  SetValue(&is_visible_, tab_id, new_visibility);

  return true;
}

void ExtensionAction::DeclarativeShow(int tab_id) {
  DCHECK_NE(tab_id, kDefaultTabId);
  ++declarative_show_count_[tab_id];  // Use default initialization to 0.
}

void ExtensionAction::UndoDeclarativeShow(int tab_id) {
  int& show_count = declarative_show_count_[tab_id];
  DCHECK_GT(show_count, 0);
  if (--show_count == 0)
    declarative_show_count_.erase(tab_id);
}

void ExtensionAction::ClearAllValuesForTab(int tab_id) {
  popup_url_.erase(tab_id);
  title_.erase(tab_id);
  icon_.erase(tab_id);
  badge_text_.erase(tab_id);
  badge_text_color_.erase(tab_id);
  badge_background_color_.erase(tab_id);
  is_visible_.erase(tab_id);
  // TODO(jyasskin): Erase the element from declarative_show_count_
  // when the tab's closed.  There's a race between the
  // PageActionController and the ContentRulesRegistry on navigation,
  // which prevents me from cleaning everything up now.
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

bool ExtensionAction::HasPopupUrl(int tab_id) const {
  return HasValue(popup_url_, tab_id);
}

bool ExtensionAction::HasTitle(int tab_id) const {
  return HasValue(title_, tab_id);
}

bool ExtensionAction::HasBadgeText(int tab_id) const {
  return HasValue(badge_text_, tab_id);
}

bool ExtensionAction::HasBadgeBackgroundColor(int tab_id) const {
  return HasValue(badge_background_color_, tab_id);
}

bool ExtensionAction::HasBadgeTextColor(int tab_id) const {
  return HasValue(badge_text_color_, tab_id);
}

bool ExtensionAction::HasIsVisible(int tab_id) const {
  return HasValue(is_visible_, tab_id);
}

bool ExtensionAction::HasIcon(int tab_id) const {
  return HasValue(icon_, tab_id);
}

// Determines which icon would be returned by |GetIcon|, and returns its width.
int ExtensionAction::GetIconWidth(int tab_id) const {
  // If icon has been set, return its width.
  gfx::ImageSkia icon = GetValue(&icon_, tab_id);
  if (!icon.isNull())
    return icon.width();
  // If there is a default icon, the icon width will be set depending on our
  // action type.
  if (default_icon_)
    return GetIconSizeForType(action_type());

  // If no icon has been set and there is no default icon, we need favicon
  // width.
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_EXTENSIONS_FAVICON).ToImageSkia()->width();
}
