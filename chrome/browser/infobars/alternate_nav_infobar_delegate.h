// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_ALTERNATE_NAV_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_INFOBARS_ALTERNATE_NAV_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/api/infobars/infobar_delegate.h"
#include "googleurl/src/gurl.h"

class AlternateNavInfoBarDelegate : public InfoBarDelegate {
 public:
  AlternateNavInfoBarDelegate(InfoBarService* owner,
                              const GURL& alternate_nav_url);
  virtual ~AlternateNavInfoBarDelegate();

  string16 GetMessageTextWithOffset(size_t* link_offset) const;
  string16 GetLinkText() const;
  bool LinkClicked(WindowOpenDisposition disposition);

 private:
  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar(InfoBarService* owner) OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual AlternateNavInfoBarDelegate* AsAlternateNavInfoBarDelegate() OVERRIDE;

  GURL alternate_nav_url_;

  DISALLOW_COPY_AND_ASSIGN(AlternateNavInfoBarDelegate);
};

#endif  // CHROME_BROWSER_INFOBARS_ALTERNATE_NAV_INFOBAR_DELEGATE_H_
