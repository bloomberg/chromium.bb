// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INSTALLER_H_
#define CHROME_BROWSER_PLUGIN_INSTALLER_H_
#pragma once

#include "chrome/browser/tab_contents/infobar_delegate.h"

class TabContents;

// The main purpose for this class is to popup/close the infobar when there is
// a missing plugin.
class PluginInstaller : public ConfirmInfoBarDelegate {
 public:
  explicit PluginInstaller(TabContents* tab_contents);
  ~PluginInstaller();

  void OnMissingPluginStatus(int status);
  // A new page starts loading. This is the perfect time to close the info bar.
  void OnStartLoading();

 private:
  // Overridden from ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const;
  virtual SkBitmap* GetIcon() const;
  virtual int GetButtons() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual string16 GetLinkText();
  virtual bool LinkClicked(WindowOpenDisposition disposition);

  // The containing TabContents
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstaller);
};

#endif  // CHROME_BROWSER_PLUGIN_INSTALLER_H_
