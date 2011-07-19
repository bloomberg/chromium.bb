// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

// Size of extension icon in top left of dialog.
static const int kIconSize = 69;

ExtensionUninstallDialog::ExtensionUninstallDialog(Profile* profile)
    : profile_(profile),
      ui_loop_(MessageLoop::current()),
      delegate_(NULL),
      extension_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {
}

ExtensionUninstallDialog::~ExtensionUninstallDialog() {
}

void ExtensionUninstallDialog::ConfirmUninstall(Delegate* delegate,
                                                const Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  delegate_ = delegate;
  extension_ = extension;

  ExtensionResource image =
      extension_->GetIconResource(Extension::EXTENSION_ICON_LARGE,
                                  ExtensionIconSet::MATCH_EXACTLY);
  // Load the image asynchronously. The response will be sent to OnImageLoaded.
  tracker_.LoadImage(extension_, image,
                     gfx::Size(kIconSize, kIconSize),
                     ImageLoadingTracker::DONT_CACHE);
}

void ExtensionUninstallDialog::SetIcon(SkBitmap* image) {
  if (image)
    icon_ = *image;
  else
    icon_ = SkBitmap();
  if (icon_.empty()) {
    if (extension_->is_app()) {
      icon_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_APP_DEFAULT_ICON);
    } else {
      icon_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_EXTENSION_DEFAULT_ICON);
    }
  }
}

void ExtensionUninstallDialog::OnImageLoaded(SkBitmap* image,
                                             const ExtensionResource& resource,
                                             int index) {
  SetIcon(image);

  Show(profile_, delegate_, extension_, &icon_);
}
