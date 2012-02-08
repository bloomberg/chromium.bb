// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INSTALLER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PLUGIN_INSTALLER_INFOBAR_DELEGATE_H_
#pragma once

#include "base/callback.h"
#include "chrome/browser/plugin_installer_observer.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/link_infobar_delegate.h"
#include "googleurl/src/gurl.h"

// The main purpose for this class is to popup/close the infobar when there is
// a missing plugin.
class PluginInstallerInfoBarDelegate : public ConfirmInfoBarDelegate,
                                       public WeakPluginInstallerObserver {
 public:
  // Shows an infobar asking whether to install the plugin represented by
  // |installer|. When the user accepts, |callback| is called.
  // During installation of the plug-in, the infobar will change to reflect the
  // installation state.
  static InfoBarDelegate* Create(InfoBarTabHelper* infobar_helper,
                                 PluginInstaller* installer,
                                 const base::Closure& callback);

 private:
  PluginInstallerInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                 PluginInstaller* installer,
                                 const base::Closure& callback,
                                 const string16& message);
  virtual ~PluginInstallerInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  // PluginInstallerObserver:
  virtual void DidStartDownload() OVERRIDE;
  virtual void DidFinishDownload() OVERRIDE;
  virtual void DownloadError(const std::string& message) OVERRIDE;

  // WeakPluginInstallerObserver:
  virtual void OnlyWeakObserversLeft() OVERRIDE;

  // Replaces this infobar with one showing |message|. The new infobar will
  // not have any buttons (and not call the callback).
  void ReplaceWithInfoBar(const string16& message);

  base::Closure callback_;

  string16 message_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstallerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PLUGIN_INSTALLER_INFOBAR_DELEGATE_H_
