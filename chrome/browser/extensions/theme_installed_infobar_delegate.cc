// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

ThemeInstalledInfoBarDelegate::ThemeInstalledInfoBarDelegate(
    TabContents* tab_contents, const Extension* new_theme,
    const std::string& previous_theme_id)
         : ConfirmInfoBarDelegate(tab_contents),
           was_canceled_(false),
           profile_(tab_contents->profile()),
           name_(new_theme->name()),
           new_theme_id_(new_theme->id()),
           previous_theme_id_(previous_theme_id) {
}

void ThemeInstalledInfoBarDelegate::InfoBarClosed() {
  ExtensionsService* service = profile_->GetExtensionsService();
  if (service) {
    std::string uninstall_id;
    if (was_canceled_)
      uninstall_id = new_theme_id_;
    else if (!previous_theme_id_.empty())
      uninstall_id = previous_theme_id_;
    // It's possible that the theme was already uninstalled by someone so make
    // sure it exists.
    if (!uninstall_id.empty() && service->GetExtensionById(uninstall_id))
      service->UninstallExtension(uninstall_id, false);
  }
  delete this;
}

std::wstring ThemeInstalledInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringF(IDS_THEME_INSTALL_INFOBAR_LABEL,
                               UTF8ToWide(name_));
}

SkBitmap* ThemeInstalledInfoBarDelegate::GetIcon() const {
  // TODO(aa): Reply with the theme's icon, but this requires reading it
  // asynchronously from disk.
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_THEME);
}

ThemeInstalledInfoBarDelegate*
    ThemeInstalledInfoBarDelegate::AsThemePreviewInfobarDelegate() {
  return this;
}

int ThemeInstalledInfoBarDelegate::GetButtons() const {
  return BUTTON_CANCEL;
}

std::wstring ThemeInstalledInfoBarDelegate::GetButtonLabel(
    ConfirmInfoBarDelegate::InfoBarButton button) const {
  switch (button) {
    case BUTTON_CANCEL: {
      return l10n_util::GetString(IDS_THEME_INSTALL_INFOBAR_UNDO_BUTTON);
    }
    default:
      NOTREACHED();
      return L"";
  }
}

bool ThemeInstalledInfoBarDelegate::Cancel() {
  was_canceled_ = true;
  if (!previous_theme_id_.empty()) {
    ExtensionsService* service = profile_->GetExtensionsService();
    if (service) {
      Extension* previous_theme = service->GetExtensionById(previous_theme_id_);
      if (previous_theme) {
        profile_->SetTheme(previous_theme);
        return true;
      }
    }
  }

  profile_->ClearTheme();
  return true;
}
