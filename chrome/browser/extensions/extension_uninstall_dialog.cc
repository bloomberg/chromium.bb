// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/extension_resource.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#endif

namespace {

// Returns pixel size under maximal scale factor for the icon whose device
// independent size is |size_in_dip|
int GetSizeForMaxScaleFactor(int size_in_dip) {
  ui::ScaleFactor max_scale_factor = ui::GetMaxScaleFactor();
  float max_scale_factor_scale = ui::GetScaleFactorScale(max_scale_factor);

  return static_cast<int>(size_in_dip * max_scale_factor_scale);
}

// Returns bitmap for the default icon with size equal to the default icon's
// pixel size under maximal supported scale factor.
SkBitmap GetDefaultIconBitmapForMaxScaleFactor(bool is_app) {
  const gfx::ImageSkia& image = is_app ?
      extensions::IconsInfo::GetDefaultAppIcon() :
      extensions::IconsInfo::GetDefaultExtensionIcon();
  return image.GetRepresentation(ui::GetMaxScaleFactor()).sk_bitmap();
}

}  // namespace

// Size of extension icon in top left of dialog.
static const int kIconSize = 69;

ExtensionUninstallDialog::ExtensionUninstallDialog(
    Profile* profile,
    Browser* browser,
    ExtensionUninstallDialog::Delegate* delegate)
    : profile_(profile),
      browser_(browser),
      delegate_(delegate),
      extension_(NULL),
      state_(kImageIsLoading),
      ui_loop_(MessageLoop::current()) {
  if (browser) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_BROWSER_CLOSED,
                   content::Source<Browser>(browser));
  }
}

ExtensionUninstallDialog::~ExtensionUninstallDialog() {
}

void ExtensionUninstallDialog::ConfirmUninstall(
    const extensions::Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;

#if defined(ENABLE_MANAGED_USERS)
  // If the profile belongs to a managed user, and the profile is not in
  // elevated state, a passphrase dialog is shown, and if the custodian
  // authorizes by entering his passphrase, the uninstall is continued by
  // calling |ExtensionUninstallAccepted| on the delegate.
  if (ShowAuthorizationDialog())
    return;
#endif

  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      extension_,
      extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER);
  // Load the icon whose pixel size is large enough to be displayed under
  // maximal supported scale factor. UI code will scale the icon down if needed.
  int pixel_size = GetSizeForMaxScaleFactor(kIconSize);

  // Load the image asynchronously. The response will be sent to OnImageLoaded.
  state_ = kImageIsLoading;
  extensions::ImageLoader* loader =
      extensions::ImageLoader::Get(profile_);
  loader->LoadImageAsync(extension_, image,
                         gfx::Size(pixel_size, pixel_size),
                         base::Bind(&ExtensionUninstallDialog::OnImageLoaded,
                                    AsWeakPtr()));
}

void ExtensionUninstallDialog::SetIcon(const gfx::Image& image) {
  if (image.IsEmpty()) {
    // Let's set default icon bitmap whose size is equal to the default icon's
    // pixel size under maximal supported scale factor. If the bitmap is larger
    // than the one we need, it will be scaled down by the ui code.
    // TODO(tbarzic): We should use IconImage here and load the required bitmap
    //     lazily.
    icon_ = gfx::ImageSkia::CreateFrom1xBitmap(
        GetDefaultIconBitmapForMaxScaleFactor(extension_->is_app()));
  } else {
    icon_ = *image.ToImageSkia();
  }
}

void ExtensionUninstallDialog::OnImageLoaded(const gfx::Image& image) {
  SetIcon(image);

  // Show the dialog unless the browser has been closed while we were waiting
  // for the image.
  DCHECK(state_ == kImageIsLoading || state_ == kBrowserIsClosing);
  if (state_ == kImageIsLoading) {
    state_ = kDialogIsShowing;
    Show();
  }
}

void ExtensionUninstallDialog::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_CLOSED);

  browser_ = NULL;
  // If the browser is closed while waiting for the image, we need to send a
  // "cancel" event here, because there will not be another opportunity to
  // notify the delegate of the cancellation as we won't open the dialog.
  if (state_ == kImageIsLoading) {
    state_ = kBrowserIsClosing;
    delegate_->ExtensionUninstallCanceled();
  }
}

#if defined(ENABLE_MANAGED_USERS)
bool ExtensionUninstallDialog::ShowAuthorizationDialog() {
  ManagedUserService* service =
      ManagedUserServiceFactory::GetForProfile(profile_);
  if (service->ProfileIsManaged() && !service->CanSkipPassphraseDialog()) {
    service->RequestAuthorizationUsingActiveWebContents(
        browser_,
        base::Bind(&ExtensionUninstallDialog::OnAuthorizationResult,
                   base::Unretained(this)));
    return true;
  }
  return false;
}

void ExtensionUninstallDialog::OnAuthorizationResult(bool success) {
  if (success) {
    ManagedUserService* service = ManagedUserServiceFactory::GetForProfile(
        profile_);
    DCHECK(service);
    service->AddElevationForExtension(extension_->id());
    delegate_->ExtensionUninstallAccepted();
  } else {
    delegate_->ExtensionUninstallCanceled();
  }
}
#endif
