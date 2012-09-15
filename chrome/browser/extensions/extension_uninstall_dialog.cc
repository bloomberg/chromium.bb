// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

// Returns pixel size under maximal scale factor for the icon whose device
// independent size is |size_in_dip|
int GetSizeForMaxScaleFactor(int size_in_dip) {
  std::vector<ui::ScaleFactor> supported_scale_factors =
      ui::GetSupportedScaleFactors();
  // Scale factors are in ascending order, so the last one is the one we need.
  ui::ScaleFactor max_scale_factor = supported_scale_factors.back();
  float max_scale_factor_scale = ui::GetScaleFactorScale(max_scale_factor);

  return static_cast<int>(size_in_dip * max_scale_factor_scale);
}

// Returns bitmap for the default icon with size equal to the default icon's
// pixel size under maximal supported scale factor.
SkBitmap GetDefaultIconBitmapForMaxScaleFactor(bool is_app) {
  std::vector<ui::ScaleFactor> supported_scale_factors =
      ui::GetSupportedScaleFactors();
  // Scale factors are in ascending order, so the last one is the one we need.
  ui::ScaleFactor max_scale_factor =
      supported_scale_factors[supported_scale_factors.size() - 1];

  return extensions::Extension::GetDefaultIcon(is_app).
      GetRepresentation(max_scale_factor).sk_bitmap();
}

}  // namespace

// Size of extension icon in top left of dialog.
static const int kIconSize = 69;

ExtensionUninstallDialog::ExtensionUninstallDialog(
    Browser* browser,
    ExtensionUninstallDialog::Delegate* delegate)
    : browser_(browser),
      delegate_(delegate),
      extension_(NULL),
      ui_loop_(MessageLoop::current()) {
  if (browser) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_BROWSER_CLOSING,
                   content::Source<Browser>(browser));
  }
}

ExtensionUninstallDialog::~ExtensionUninstallDialog() {
}

void ExtensionUninstallDialog::ConfirmUninstall(
    const extensions::Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;

  ExtensionResource image =
      extension_->GetIconResource(extension_misc::EXTENSION_ICON_LARGE,
                                  ExtensionIconSet::MATCH_BIGGER);
  // Load the image asynchronously. The response will be sent to OnImageLoaded.
  tracker_.reset(new ImageLoadingTracker(this));
  // Load the icon whose pixel size is large enough to be displayed under
  // maximal supported scale factor. UI code will scale the icon down if needed.
  int pixel_size = GetSizeForMaxScaleFactor(kIconSize);
  tracker_->LoadImage(extension_, image,
                      gfx::Size(pixel_size, pixel_size),
                      ImageLoadingTracker::DONT_CACHE);
}

void ExtensionUninstallDialog::SetIcon(const gfx::Image& image) {
  if (image.IsEmpty()) {
    // Let's set default icon bitmap whose size is equal to the default icon's
    // pixel size under maximal supported scale factor. If the bitmap is larger
    // than the one we need, it will be scaled down by the ui code.
    // TODO(tbarzic): We should use IconImage here and load the required bitmap
    //     lazily.
    icon_ = GetDefaultIconBitmapForMaxScaleFactor(extension_->is_app());
  } else {
    icon_ = *image.ToImageSkia();
  }
}

void ExtensionUninstallDialog::OnImageLoaded(const gfx::Image& image,
                                             const std::string& extension_id,
                                             int index) {
  SetIcon(image);

  // Reset the tracker so that we can use its presence as a signal that we're
  // still waiting for the icon to load.
  tracker_.reset();

  Show();
}

void ExtensionUninstallDialog::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_CLOSING);

  browser_ = NULL;
  if (tracker_.get()) {
    // If we're waiting for the icon, stop doing so because we're not going to
    // show the dialog.
    tracker_.reset();
    delegate_->ExtensionUninstallCanceled();
  }
}
