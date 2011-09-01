// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

ThemeInstalledInfoBarDelegate::ThemeInstalledInfoBarDelegate(
    TabContents* tab_contents,
    const Extension* new_theme,
    const std::string& previous_theme_id,
    bool previous_using_native_theme)
    : ConfirmInfoBarDelegate(tab_contents),
      profile_(Profile::FromBrowserContext(tab_contents->browser_context())),
      theme_service_(ThemeServiceFactory::GetForProfile(profile_)),
      name_(new_theme->name()),
      theme_id_(new_theme->id()),
      previous_theme_id_(previous_theme_id),
      previous_using_native_theme_(previous_using_native_theme) {
  theme_service_->OnInfobarDisplayed();
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 Source<ThemeService>(theme_service_));
}

bool ThemeInstalledInfoBarDelegate::MatchesTheme(const Extension* theme) const {
  return theme && (theme->id() == theme_id_);
}

ThemeInstalledInfoBarDelegate::~ThemeInstalledInfoBarDelegate() {
  // We don't want any notifications while we're running our destructor.
  registrar_.RemoveAll();

  theme_service_->OnInfobarDestroyed();
}

bool ThemeInstalledInfoBarDelegate::Cancel() {
  if (!previous_theme_id_.empty()) {
    ExtensionService* service = profile_->GetExtensionService();
    if (service) {
      const Extension* previous_theme =
          service->GetExtensionById(previous_theme_id_, true);
      if (previous_theme) {
        theme_service_->SetTheme(previous_theme);
        return true;
      }
    }
  }

  if (previous_using_native_theme_) {
    theme_service_->SetNativeTheme();
  } else {
    theme_service_->UseDefaultTheme();
  }
  return true;
}

gfx::Image* ThemeInstalledInfoBarDelegate::GetIcon() const {
  // TODO(aa): Reply with the theme's icon, but this requires reading it
  // asynchronously from disk.
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_THEME);
}

InfoBarDelegate::Type ThemeInstalledInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

ThemeInstalledInfoBarDelegate*
    ThemeInstalledInfoBarDelegate::AsThemePreviewInfobarDelegate() {
  return this;
}

string16 ThemeInstalledInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_THEME_INSTALL_INFOBAR_LABEL,
                                    UTF8ToUTF16(name_));
}

int ThemeInstalledInfoBarDelegate::GetButtons() const {
  return BUTTON_CANCEL;
}

string16 ThemeInstalledInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_CANCEL, button);
  return l10n_util::GetStringUTF16(IDS_THEME_INSTALL_INFOBAR_UNDO_BUTTON);
}

void ThemeInstalledInfoBarDelegate::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  // If the new theme is different from what this info bar is associated with,
  // close this info bar since it is no longer relevant.
  if (theme_id_ != theme_service_->GetThemeID())
    RemoveSelf();
}
