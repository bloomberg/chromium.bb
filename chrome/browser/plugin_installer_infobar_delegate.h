// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INSTALLER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PLUGIN_INSTALLER_INFOBAR_DELEGATE_H_
#pragma once

#include "chrome/browser/tab_contents/infobar_delegate.h"

class TabContents;

// The main purpose for this class is to popup/close the infobar when there is
// a missing plugin.
class PluginInstallerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit PluginInstallerInfoBarDelegate(TabContents* tab_contents);

 private:
  virtual ~PluginInstallerInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual SkBitmap* GetIcon() const;
  virtual PluginInstallerInfoBarDelegate* AsPluginInstallerInfoBarDelegate();
  virtual string16 GetMessageText() const;
  virtual int GetButtons() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual string16 GetLinkText();
  virtual bool LinkClicked(WindowOpenDisposition disposition);

  // The containing TabContents
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstallerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PLUGIN_INSTALLER_INFOBAR_DELEGATE_H_
