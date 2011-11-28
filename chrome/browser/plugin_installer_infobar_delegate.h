// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INSTALLER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PLUGIN_INSTALLER_INFOBAR_DELEGATE_H_
#pragma once

#include "base/callback.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "googleurl/src/gurl.h"

// The main purpose for this class is to popup/close the infobar when there is
// a missing plugin.
class PluginInstallerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Shows an infobar asking whether to install the plugin with the name
  // |plugin_name|. When the user accepts, |callback| is called.
  PluginInstallerInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                 const string16& plugin_name,
                                 const GURL& learn_more_url,
                                 const base::Closure& callback);

 private:
  virtual ~PluginInstallerInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual PluginInstallerInfoBarDelegate*
      AsPluginInstallerInfoBarDelegate() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  string16 plugin_name_;
  GURL learn_more_url_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstallerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PLUGIN_INSTALLER_INFOBAR_DELEGATE_H_
