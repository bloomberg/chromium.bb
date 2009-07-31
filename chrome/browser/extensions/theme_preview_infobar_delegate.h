// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_THEME_PREVIEW_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_THEME_PREVIEW_INFOBAR_DELEGATE_H_

#include "chrome/browser/tab_contents/infobar_delegate.h"

class Extension;
class ExtensionsService;
class SkBitmap;
class TabContents;

// When a user installs a theme, we display it immediately, but provide an
// infobar allowing them to cancel.
class ThemePreviewInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  ThemePreviewInfobarDelegate(TabContents* tab_contents,
                              const std::string& name);
  virtual void InfoBarClosed();
  virtual std::wstring GetMessageText() const;
  virtual SkBitmap* GetIcon() const;
  virtual ThemePreviewInfobarDelegate* AsThemePreviewInfobarDelegate();
  virtual int GetButtons() const;
  virtual std::wstring GetButtonLabel(
      ConfirmInfoBarDelegate::InfoBarButton button) const;
  virtual bool Cancel();

 private:
  Profile* profile_;
  std::string name_;  // name of theme to install
};

#endif  // CHROME_BROWSER_VIEWS_EXTENSIONS_THEME_PREVIEW_INFOBAR_DELEGATE_H_
