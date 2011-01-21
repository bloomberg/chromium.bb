// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
#pragma once

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/common/notification_registrar.h"

class Extension;
class SkBitmap;
class TabContents;

// When a user installs a theme, we display it immediately, but provide an
// infobar allowing them to cancel.
class ThemeInstalledInfoBarDelegate : public ConfirmInfoBarDelegate,
                                      public NotificationObserver {
 public:
  ThemeInstalledInfoBarDelegate(TabContents* tab_contents,
                                const Extension* new_theme,
                                const std::string& previous_theme_id);

  // Returns true if the given theme is the same as the one associated with this
  // info bar.
  bool MatchesTheme(const Extension* theme);

 protected:
  virtual ~ThemeInstalledInfoBarDelegate();

  Profile* profile() { return profile_; }

  // ConfirmInfoBarDelegate:
  virtual bool Cancel();

 private:
  // ConfirmInfoBarDelegate:
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual ThemeInstalledInfoBarDelegate* AsThemePreviewInfobarDelegate();
  virtual string16 GetMessageText() const;
  virtual int GetButtons() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Profile* profile_;

  // Name of theme that's just been installed.
  std::string name_;

  // ID of theme that's just been installed.
  std::string theme_id_;

  // Used to undo theme install.
  std::string previous_theme_id_;

  // Tab to which this info bar is associated.
  TabContents* tab_contents_;

  // Registers and unregisters us for notifications.
  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
