// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ThemeService;
class Extension;
class Profile;
class SkBitmap;

// When a user installs a theme, we display it immediately, but provide an
// infobar allowing them to cancel.
class ThemeInstalledInfoBarDelegate : public ConfirmInfoBarDelegate,
                                      public NotificationObserver {
 public:
  ThemeInstalledInfoBarDelegate(TabContents* tab_contents,
                                const Extension* new_theme,
                                const std::string& previous_theme_id,
                                bool previous_using_native_theme);

  // Returns true if the given theme is the same as the one associated with this
  // info bar.
  bool MatchesTheme(const Extension* theme) const;

 protected:
  virtual ~ThemeInstalledInfoBarDelegate();

  ThemeService* theme_service() { return theme_service_; }

  // ConfirmInfoBarDelegate:
  virtual bool Cancel() OVERRIDE;

 private:
  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual ThemeInstalledInfoBarDelegate*
      AsThemePreviewInfobarDelegate() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  Profile* profile_;
  ThemeService* theme_service_;

  // Name of theme that's just been installed.
  std::string name_;

  // ID of theme that's just been installed.
  std::string theme_id_;

  // Used to undo theme install.
  std::string previous_theme_id_;
  bool previous_using_native_theme_;

  // Registers and unregisters us for notifications.
  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
