// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_icon_factory.h"

#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using extensions::Extension;
using extensions::IconImage;

namespace {

gfx::ImageSkia GetDefaultIcon() {
  return *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_EXTENSIONS_FAVICON).ToImageSkia();
}

}  // namespace

ExtensionActionIconFactory::ExtensionActionIconFactory(
    Profile* profile,
    const Extension* extension,
    const ExtensionAction* action,
    Observer* observer)
    : extension_(extension),
      action_(action),
      observer_(observer) {
  if (action_->default_icon()) {
    default_icon_.reset(new IconImage(
        profile,
        extension_,
        *action_->default_icon(),
        ExtensionAction::GetIconSizeForType(action_->action_type()),
        GetDefaultIcon(),
        this));
  }
}

ExtensionActionIconFactory::~ExtensionActionIconFactory() {}

// extensions::IconImage::Observer overrides.
void ExtensionActionIconFactory::OnExtensionIconImageChanged(IconImage* image) {
  if (observer_)
    observer_->OnIconUpdated();
}

gfx::Image ExtensionActionIconFactory::GetIcon(int tab_id) {
  gfx::ImageSkia base_icon = GetBaseIconFromAction(tab_id);
  return gfx::Image(base_icon);
}

gfx::ImageSkia ExtensionActionIconFactory::GetBaseIconFromAction(int tab_id) {
  gfx::ImageSkia icon = action_->GetExplicitlySetIcon(tab_id);
  if (!icon.isNull())
    return icon;

  if (default_icon_)
    return default_icon_->image_skia();

  return GetDefaultIcon();
}
