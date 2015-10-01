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

#if defined(ENABLE_PLUGIN_INSTALLATION)
// Infobar that's shown when a plugin is out of date.
class OutdatedPluginInfoBarDelegate : public ConfirmInfoBarDelegate,
                                      public WeakPluginInstallerObserver {
 public:
  // Creates an outdated plugin infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     PluginInstaller* installer,
                     scoped_ptr<PluginMetadata> metadata);

  // Replaces |infobar|, which must currently be owned, with an infobar asking
  // the user to update a particular plugin.
  static void Replace(infobars::InfoBar* infobar,
                      PluginInstaller* installer,
                      scoped_ptr<PluginMetadata> plugin_metadata,
                      const base::string16& message);

 private:
  OutdatedPluginInfoBarDelegate(PluginInstaller* installer,
                                scoped_ptr<PluginMetadata> metadata,
                                const base::string16& message);
  ~OutdatedPluginInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  void InfoBarDismissed() override;
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;

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

  std::string identifier_;

  scoped_ptr<PluginMetadata> plugin_metadata_;

  base::string16 message_;

  DISALLOW_COPY_AND_ASSIGN(OutdatedPluginInfoBarDelegate);
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
  ~PluginMetroModeInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;

  const Mode mode_;
  const base::string16 name_;

  DISALLOW_COPY_AND_ASSIGN(PluginMetroModeInfoBarDelegate);
};
#endif  // defined(OS_WIN)

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INFOBAR_DELEGATES_H_
