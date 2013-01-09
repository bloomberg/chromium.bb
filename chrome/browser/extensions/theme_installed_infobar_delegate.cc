// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"


// static
void ThemeInstalledInfoBarDelegate::Create(
    const extensions::Extension* new_theme,
    Profile* profile,
    const std::string& previous_theme_id,
    bool previous_using_native_theme) {
  if (!new_theme->is_theme())
    return;

  // Get last active tabbed browser of profile.
  Browser* browser = chrome::FindTabbedBrowser(profile,
                                               true,
                                               chrome::GetActiveDesktop());
  if (!browser)
    return;

  content::WebContents* web_contents = chrome::GetActiveWebContents(browser);
  if (!web_contents)
    return;
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);

  // First find any previous theme preview infobars.
  InfoBarDelegate* old_delegate = NULL;
  for (size_t i = 0; i < infobar_service->GetInfoBarCount(); ++i) {
    InfoBarDelegate* delegate = infobar_service->GetInfoBarDelegateAt(i);
    ThemeInstalledInfoBarDelegate* theme_infobar =
        delegate->AsThemePreviewInfobarDelegate();
    if (theme_infobar) {
      // If the user installed the same theme twice, ignore the second install
      // and keep the first install info bar, so that they can easily undo to
      // get back the previous theme.
      if (theme_infobar->theme_id_ == new_theme->id())
        return;
      old_delegate = delegate;
      break;
    }
  }

  // Then either replace that old one or add a new one.
  scoped_ptr<InfoBarDelegate> new_delegate(
      new ThemeInstalledInfoBarDelegate(
          infobar_service,
          profile->GetExtensionService(),
          ThemeServiceFactory::GetForProfile(profile),
          new_theme,
          previous_theme_id,
          previous_using_native_theme));

  if (old_delegate)
    infobar_service->ReplaceInfoBar(old_delegate, new_delegate.Pass());
  else
    infobar_service->AddInfoBar(new_delegate.Pass());
}

bool ThemeInstalledInfoBarDelegate::Cancel() {
  if (!previous_theme_id_.empty()) {
    const extensions::Extension* previous_theme =
        extension_service_->GetExtensionById(previous_theme_id_, true);
    if (previous_theme) {
      theme_service_->SetTheme(previous_theme);
        return false;  // The theme change will close us.
    }
  }

  if (previous_using_native_theme_)
    theme_service_->SetNativeTheme();
  else
    theme_service_->UseDefaultTheme();
  return false;  // The theme change will close us.
}

ThemeInstalledInfoBarDelegate::ThemeInstalledInfoBarDelegate(
    InfoBarService* infobar_service,
    ExtensionService* extension_service,
    ThemeService* theme_service,
    const extensions::Extension* new_theme,
    const std::string& previous_theme_id,
    bool previous_using_native_theme)
    : ConfirmInfoBarDelegate(infobar_service),
      extension_service_(extension_service),
      theme_service_(theme_service),
      name_(new_theme->name()),
      theme_id_(new_theme->id()),
      previous_theme_id_(previous_theme_id),
      previous_using_native_theme_(previous_using_native_theme) {
  theme_service_->OnInfobarDisplayed();
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
}

ThemeInstalledInfoBarDelegate::~ThemeInstalledInfoBarDelegate() {
  // We don't want any notifications while we're running our destructor.
  registrar_.RemoveAll();

  theme_service_->OnInfobarDestroyed();
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
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  // If the new theme is different from what this info bar is associated with,
  // close this info bar since it is no longer relevant.
  if (theme_id_ != theme_service_->GetThemeID())
    RemoveSelf();
}
