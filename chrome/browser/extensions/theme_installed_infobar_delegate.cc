// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

ThemeInstalledInfoBarDelegate::ThemeInstalledInfoBarDelegate(
    TabContents* tab_contents,
    const Extension* new_theme,
    const std::string& previous_theme_id)
    : ConfirmInfoBarDelegate(tab_contents),
      profile_(tab_contents->profile()),
      name_(new_theme->name()),
      theme_id_(new_theme->id()),
      previous_theme_id_(previous_theme_id),
      tab_contents_(tab_contents) {
  profile_->GetThemeProvider()->OnInfobarDisplayed();
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
}

bool ThemeInstalledInfoBarDelegate::MatchesTheme(const Extension* theme) const {
  return theme && (theme->id() == theme_id_);
}

ThemeInstalledInfoBarDelegate::~ThemeInstalledInfoBarDelegate() {
  // We don't want any notifications while we're running our destructor.
  registrar_.RemoveAll();

  profile_->GetThemeProvider()->OnInfobarDestroyed();
}

bool ThemeInstalledInfoBarDelegate::Cancel() {
  if (!previous_theme_id_.empty()) {
    ExtensionService* service = profile_->GetExtensionService();
    if (service) {
      const Extension* previous_theme =
          service->GetExtensionById(previous_theme_id_, true);
      if (previous_theme) {
        profile_->SetTheme(previous_theme);
        return true;
      }
    }
  }

  profile_->ClearTheme();
  return true;
}

void ThemeInstalledInfoBarDelegate::InfoBarClosed() {
  delete this;
}

SkBitmap* ThemeInstalledInfoBarDelegate::GetIcon() const {
  // TODO(aa): Reply with the theme's icon, but this requires reading it
  // asynchronously from disk.
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_INFOBAR_THEME);
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
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK_EQ(NotificationType::BROWSER_THEME_CHANGED, type.value);
  // If the new theme is different from what this info bar is associated
  // with, close this info bar since it is no longer relevant.
  const Extension* extension = Details<const Extension>(details).ptr();
  if (!extension || theme_id_ != extension->id()) {
    if (tab_contents_ && !tab_contents_->is_being_destroyed()) {
      tab_contents_->RemoveInfoBar(this);
      // The infobar is gone so there is no reason for this delegate to keep
      // a pointer to the TabContents (the TabContents has deleted its
      // reference to this delegate and a new delegate will be created if
      // a new infobar is created).
      tab_contents_ = NULL;
      // Although it's not being used anymore, this delegate is never deleted.
      // It can not be deleted now because it is still needed if we
      // "undo" the theme change that triggered this notification
      // (when InfoBar::OnBackgroundExpose() is called). This will likely
      // be fixed when infobar delegate deletion is cleaned up for
      // http://crbug.com/62154.
    }
  }
}
