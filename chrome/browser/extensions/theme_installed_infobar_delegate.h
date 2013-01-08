// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExtensionService;
class InfoBarService;
class Profile;
class ThemeService;

namespace extensions {
class Extension;
}

// When a user installs a theme, we display it immediately, but provide an
// infobar allowing them to cancel.
class ThemeInstalledInfoBarDelegate : public ConfirmInfoBarDelegate,
                                      public content::NotificationObserver {
 public:
  // Creates a theme installed delegate and adds it to the last active tab on
  // |profile|.
  static void Create(const extensions::Extension* new_theme,
                     Profile* profile,
                     const std::string& previous_theme_id,
                     bool previous_using_native_theme);

 protected:
  ThemeService* theme_service() { return theme_service_; }

  // ConfirmInfoBarDelegate:
  virtual bool Cancel() OVERRIDE;

 private:
  ThemeInstalledInfoBarDelegate(InfoBarService* infobar_service,
                                ExtensionService* extension_service,
                                ThemeService* theme_service,
                                const extensions::Extension* new_theme,
                                const std::string& previous_theme_id,
                                bool previous_using_native_theme);
  virtual ~ThemeInstalledInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual ThemeInstalledInfoBarDelegate*
      AsThemePreviewInfobarDelegate() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  ExtensionService* extension_service_;
  ThemeService* theme_service_;

  // Name of theme that's just been installed.
  std::string name_;

  // ID of theme that's just been installed.
  std::string theme_id_;

  // Used to undo theme install.
  std::string previous_theme_id_;
  bool previous_using_native_theme_;

  // Registers and unregisters us for notifications.
  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
