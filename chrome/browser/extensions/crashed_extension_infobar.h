// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CRASHED_EXTENSION_INFOBAR_H_
#define CHROME_BROWSER_EXTENSIONS_CRASHED_EXTENSION_INFOBAR_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"

class Extension;
class ExtensionService;
class SkBitmap;

// This infobar will be displayed when an extension process crashes. It allows
// the user to reload the crashed extension.
class CrashedExtensionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // |tab_contents| should point to the TabContents the infobar will be added
  // to. |extension| should be the crashed extension, and |extensions_service|
  // the ExtensionService which manages that extension.
  CrashedExtensionInfoBarDelegate(TabContents* tab_contents,
                                  ExtensionService* extensions_service,
                                  const Extension* extension);

  const std::string extension_id() { return extension_id_; }

 private:
  virtual ~CrashedExtensionInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual bool ShouldExpire(
      const NavigationController::LoadCommittedDetails& details) const;
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual CrashedExtensionInfoBarDelegate* AsCrashedExtensionInfoBarDelegate();
  virtual string16 GetMessageText() const;
  virtual int GetButtons() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();

  ExtensionService* extensions_service_;

  const std::string extension_id_;
  const std::string extension_name_;

  DISALLOW_COPY_AND_ASSIGN(CrashedExtensionInfoBarDelegate);
};

#endif  // CHROME_BROWSER_EXTENSIONS_CRASHED_EXTENSION_INFOBAR_H_
