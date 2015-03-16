// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INFOBAR_DELEGATES_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INFOBAR_DELEGATES_H_

#include "base/callback.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugin_installer_observer.h"
#endif

class InfoBarService;
class HostContentSettingsMap;
class PluginMetadata;

namespace content {
class WebContents;
}

// Base class for blocked plugin infobars.
class PluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 protected:
  explicit PluginInfoBarDelegate(const std::string& identifier);
  ~PluginInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  bool LinkClicked(WindowOpenDisposition disposition) override;

  virtual std::string GetLearnMoreURL() const = 0;

  void LoadBlockedPlugins();

 private:
  // ConfirmInfoBarDelegate:
  int GetIconID() const override;
  base::string16 GetLinkText() const override;

  std::string identifier_;

  DISALLOW_COPY_AND_ASSIGN(PluginInfoBarDelegate);
};

#if defined(ENABLE_PLUGIN_INSTALLATION)
// Infobar that's shown when a plugin is out of date.
class OutdatedPluginInfoBarDelegate : public PluginInfoBarDelegate,
                                      public WeakPluginInstallerObserver {
 public:
  // Creates an outdated plugin infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     PluginInstaller* installer,
                     scoped_ptr<PluginMetadata> metadata);

 private:
  OutdatedPluginInfoBarDelegate(PluginInstaller* installer,
                                scoped_ptr<PluginMetadata> metadata,
                                const base::string16& message);
  ~OutdatedPluginInfoBarDelegate() override;

  // PluginInfoBarDelegate:
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
  std::string GetLearnMoreURL() const override;

  // PluginInstallerObserver:
  void DownloadStarted() override;
  void DownloadError(const std::string& message) override;
  void DownloadCancelled() override;
  void DownloadFinished() override;

  // WeakPluginInstallerObserver:
  void OnlyWeakObserversLeft() override;

  // Replaces this infobar with one showing |message|. The new infobar will
  // not have any buttons (and not call the callback).
  void ReplaceWithInfoBar(const base::string16& message);

  scoped_ptr<PluginMetadata> plugin_metadata_;

  base::string16 message_;

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
  // During installation of the plugin, the infobar will change to reflect the
  // installation state.
  static void Create(InfoBarService* infobar_service,
                     PluginInstaller* installer,
                     scoped_ptr<PluginMetadata> plugin_metadata,
                     const InstallCallback& callback);

  // Replaces |infobar|, which must currently be owned, with an infobar asking
  // the user to install or update a particular plugin.
  static void Replace(infobars::InfoBar* infobar,
                      PluginInstaller* installer,
                      scoped_ptr<PluginMetadata> plugin_metadata,
                      bool new_install,
                      const base::string16& message);

 private:
  PluginInstallerInfoBarDelegate(PluginInstaller* installer,
                                 scoped_ptr<PluginMetadata> metadata,
                                 const InstallCallback& callback,
                                 bool new_install,
                                 const base::string16& message);
  ~PluginInstallerInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  int GetIconID() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  base::string16 GetLinkText() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

  // PluginInstallerObserver:
  void DownloadStarted() override;
  void DownloadError(const std::string& message) override;
  void DownloadCancelled() override;
  void DownloadFinished() override;

  // WeakPluginInstallerObserver:
  void OnlyWeakObserversLeft() override;

  // Replaces this infobar with one showing |message|. The new infobar will
  // not have any buttons (and not call the callback).
  void ReplaceWithInfoBar(const base::string16& message);

  scoped_ptr<PluginMetadata> plugin_metadata_;

  InstallCallback callback_;

  // True iff the plugin isn't installed yet.
  bool new_install_;

  base::string16 message_;

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
                     const base::string16& name);

 private:
  PluginMetroModeInfoBarDelegate(Mode mode, const base::string16& name);
  virtual ~PluginMetroModeInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const override;
  virtual base::string16 GetMessageText() const override;
  virtual int GetButtons() const override;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const override;
  virtual bool Accept() override;
  virtual base::string16 GetLinkText() const override;
  virtual bool LinkClicked(WindowOpenDisposition disposition) override;

  const Mode mode_;
  const base::string16 name_;

  DISALLOW_COPY_AND_ASSIGN(PluginMetroModeInfoBarDelegate);
};
#endif  // defined(OS_WIN)

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INFOBAR_DELEGATES_H_
