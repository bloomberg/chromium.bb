// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INFOBAR_DELEGATES_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INFOBAR_DELEGATES_H_

#include "base/callback.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "googleurl/src/gurl.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugin_installer_observer.h"
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

class InfoBarService;
class HostContentSettingsMap;
class PluginMetadata;

namespace content {
class WebContents;
}

// Base class for blocked plug-in infobars.
class PluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  PluginInfoBarDelegate(InfoBarService* infobar_service,
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
  // Creates an unauthorized plugin delegate and adds it to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     HostContentSettingsMap* content_settings,
                     const string16& name,
                     const std::string& identifier);

 private:
  UnauthorizedPluginInfoBarDelegate(InfoBarService* infobar_service,
                                    HostContentSettingsMap* content_settings,
                                    const string16& name,
                                    const std::string& identifier);
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
  // Creates an outdated plugin delegate and adds it to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     PluginInstaller* installer,
                     scoped_ptr<PluginMetadata> metadata);

 private:
  OutdatedPluginInfoBarDelegate(InfoBarService* infobar_service,
                                PluginInstaller* installer,
                                scoped_ptr<PluginMetadata> metadata,
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

  scoped_ptr<PluginMetadata> plugin_metadata_;

  string16 message_;

  DISALLOW_COPY_AND_ASSIGN(OutdatedPluginInfoBarDelegate);
};

// The main purpose for this class is to popup/close the infobar when there is
// a missing plugin.
class PluginInstallerInfoBarDelegate : public ConfirmInfoBarDelegate,
                                       public WeakPluginInstallerObserver {
 public:
  typedef base::Callback<void(const PluginMetadata*)> InstallCallback;

  // Shows an infobar asking whether to install the plugin represented by
  // |installer|. When the user accepts, |callback| is called.
  // During installation of the plug-in, the infobar will change to reflect the
  // installation state.
  static void Create(InfoBarService* infobar_service,
                     PluginInstaller* installer,
                     scoped_ptr<PluginMetadata> plugin_metadata,
                     const InstallCallback& callback);

  // Replaces |infobar|, which must currently be owned, with an infobar asking
  // the user to install or update a particular plugin.
  static void Replace(InfoBarDelegate* infobar,
                      PluginInstaller* installer,
                      scoped_ptr<PluginMetadata> metadata,
                      bool new_install,
                      const string16& message);

 private:
  PluginInstallerInfoBarDelegate(InfoBarService* infobar_service,
                                 PluginInstaller* installer,
                                 scoped_ptr<PluginMetadata> plugin_metadata,
                                 const InstallCallback& callback,
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

  scoped_ptr<PluginMetadata> plugin_metadata_;

  InstallCallback callback_;

  // True iff the plug-in isn't installed yet.
  bool new_install_;

  string16 message_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstallerInfoBarDelegate);
};
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

#if defined(OS_WIN)
class PluginMetroModeInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // The infobar can be used for two purposes: to inform the user about a
  // missing plugin or to note that a plugin only works in desktop mode.  These
  // purposes require different messages, buttons, etc.
  enum Mode {
    MISSING_PLUGIN,
    DESKTOP_MODE_REQUIRED,
  };

  // Creates a metro mode infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     Mode mode,
                     const string16& name);

 private:
  PluginMetroModeInfoBarDelegate(InfoBarService* infobar_service,
                                 Mode mode,
                                 const string16& name);
  virtual ~PluginMetroModeInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  const Mode mode_;
  const string16 name_;

  DISALLOW_COPY_AND_ASSIGN(PluginMetroModeInfoBarDelegate);
};
#endif  // defined(OS_WIN)

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INFOBAR_DELEGATES_H_
