// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_ALTERNATE_NAV_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_OMNIBOX_ALTERNATE_NAV_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/infobars/infobar_delegate.h"

class AlternateNavInfoBarDelegate : public InfoBarDelegate {
 public:
  // Creates an alternate nav infobar delegate and adds it to the infobar
  // service for |web_contents|.
  static void Create(content::WebContents* web_contents,
                     const string16& text,
                     const AutocompleteMatch& match,
                     const GURL& search_url);

  string16 GetMessageTextWithOffset(size_t* link_offset) const;
  string16 GetLinkText() const;
  bool LinkClicked(WindowOpenDisposition disposition);

 private:
  AlternateNavInfoBarDelegate(InfoBarService* owner,
                              Profile* profile,
                              const string16& text,
                              const AutocompleteMatch& match,
                              const GURL& search_url);
  virtual ~AlternateNavInfoBarDelegate();

  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar(InfoBarService* owner) OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;

  Profile* profile_;
  const string16 text_;
  const AutocompleteMatch match_;
  const GURL search_url_;

  DISALLOW_COPY_AND_ASSIGN(AlternateNavInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_ALTERNATE_NAV_INFOBAR_DELEGATE_H_
