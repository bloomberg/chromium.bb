// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_APP_METRO_INFOBAR_DELEGATE_WIN_H_
#define CHROME_BROWSER_UI_EXTENSIONS_APP_METRO_INFOBAR_DELEGATE_WIN_H_

#include "base/string16.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "ui/base/window_open_disposition.h"

class Profile;

namespace gfx {
class Image;
}

namespace chrome {

// This infobar operates by opening a new tab on about:blank, showing an
// infobar offering to relaunch the browser in metro mode, and then relaunching
// in Desktop mode if confirmed.
class AppMetroInfoBarDelegateWin : public ConfirmInfoBarDelegate {
 public:
  enum Mode {
    SHOW_APP_LIST,
    LAUNCH_PACKAGED_APP
  };

  // Creates an instance of the app metro infobar delegate, adds it to a new
  // browser tab, then activates Metro mode.
  static void Create(Profile* profile,
                     Mode mode,
                     const std::string& extension_id);

 private:
  explicit AppMetroInfoBarDelegateWin(InfoBarService* infobar_service,
                                      Mode mode,
                                      const std::string& extension_id);
  virtual ~AppMetroInfoBarDelegateWin();

  // ConfirmInfoBarDelegate overrides:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  Mode mode_;
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(AppMetroInfoBarDelegateWin);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_EXTENSIONS_APP_METRO_INFOBAR_DELEGATE_WIN_H_
