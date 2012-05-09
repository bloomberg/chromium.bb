// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INFOBAR_DELEGATES_H_
#define CHROME_BROWSER_PLUGIN_INFOBAR_DELEGATES_H_
#pragma once

#include "base/callback.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "googleurl/src/gurl.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugin_installer_observer.h"
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

class HostContentSettingsMap;
class PluginObserver;

// Base class for blocked plug-in infobars.
class PluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  PluginInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                        const string16& name,
                        const std::string& identifier);

 protected:
  virtual ~PluginInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  virtual std::string GetLearnMoreURL() const = 0;

  void LoadBlockedPlugins();

  string16 name_;

 private:
  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;

  std::string identifier_;

  DISALLOW_COPY_AND_ASSIGN(PluginInfoBarDelegate);
};

// Infobar that's shown when a plug-in requires user authorization to run.
class UnauthorizedPluginInfoBarDelegate : public PluginInfoBarDelegate {
 public:
  UnauthorizedPluginInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                    HostContentSettingsMap* content_settings,
                                    const string16& name,
                                    const std::string& identifier);

 private:
  virtual ~UnauthorizedPluginInfoBarDelegate();

  // PluginInfoBarDelegate:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;
  virtual std::string GetLearnMoreURL() const OVERRIDE;

  HostContentSettingsMap* content_settings_;

  DISALLOW_COPY_AND_ASSIGN(UnauthorizedPluginInfoBarDelegate);
};

#if defined(ENABLE_PLUGIN_INSTALLATION)
// Infobar that's shown when a plug-in is out of date.
class OutdatedPluginInfoBarDelegate : public PluginInfoBarDelegate,
                                      public WeakPluginInstallerObserver {
 public:
  static InfoBarDelegate* Create(PluginObserver* observer,
                                 PluginInstaller* installer);

 private:
  OutdatedPluginInfoBarDelegate(PluginObserver* observer,
                                PluginInstaller* installer,
                                const string16& message);
  virtual ~OutdatedPluginInfoBarDelegate();

  // PluginInfoBarDelegate:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;
  virtual std::string GetLearnMoreURL() const OVERRIDE;

  // PluginInstallerObserver:
  virtual void DownloadStarted() OVERRIDE;
  virtual void DownloadError(const std::string& message) OVERRIDE;
  virtual void DownloadCancelled() OVERRIDE;
  virtual void DownloadFinished() OVERRIDE;

  // WeakPluginInstallerObserver:
  virtual void OnlyWeakObserversLeft() OVERRIDE;

  // Replaces this infobar with one showing |message|. The new infobar will
  // not have any buttons (and not call the callback).
  void ReplaceWithInfoBar(const string16& message);

  // Has the same lifetime as TabContentsWrapper, which owns us
  // (transitively via InfoBarTabHelper).
  PluginObserver* observer_;

  string16 message_;

  DISALLOW_COPY_AND_ASSIGN(OutdatedPluginInfoBarDelegate);
};

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
  friend class OutdatedPluginInfoBarDelegate;

  PluginInstallerInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                 PluginInstaller* installer,
                                 const base::Closure& callback,
                                 bool new_install,
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
  virtual void DownloadStarted() OVERRIDE;
  virtual void DownloadError(const std::string& message) OVERRIDE;
  virtual void DownloadCancelled() OVERRIDE;
  virtual void DownloadFinished() OVERRIDE;

  // WeakPluginInstallerObserver:
  virtual void OnlyWeakObserversLeft() OVERRIDE;

  // Replaces this infobar with one showing |message|. The new infobar will
  // not have any buttons (and not call the callback).
  void ReplaceWithInfoBar(const string16& message);

  base::Closure callback_;

  // True iff the plug-in isn't installed yet.
  bool new_install_;

  string16 message_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstallerInfoBarDelegate);
};
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

#endif  // CHROME_BROWSER_PLUGIN_INFOBAR_DELEGATES_H_
