// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_ALTERNATE_NAV_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_OMNIBOX_ALTERNATE_NAV_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "url/gurl.h"

class AlternateNavInfoBarDelegate : public InfoBarDelegate {
 public:
  // Creates an alternate nav infobar delegate and adds it to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const AutocompleteMatch& match);

  string16 GetMessageTextWithOffset(size_t* link_offset) const;
  string16 GetLinkText() const;
  bool LinkClicked(WindowOpenDisposition disposition);

 private:
  AlternateNavInfoBarDelegate(InfoBarService* owner,
                              const AutocompleteMatch& match);
  virtual ~AlternateNavInfoBarDelegate();

  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar(InfoBarService* owner) OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;

  const AutocompleteMatch match_;

  DISALLOW_COPY_AND_ASSIGN(AlternateNavInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_ALTERNATE_NAV_INFOBAR_DELEGATE_H_
