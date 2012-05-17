// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/net_errors.h"
#include "googleurl/src/gurl.h"

class FilePath;
class Profile;

namespace content {
class NavigationController;
}

// Downloads and installs extensions from the web store.
class WebstoreInstaller : public content::NotificationObserver,
                          public content::DownloadItem::Observer,
                          public base::RefCounted<WebstoreInstaller> {
 public:
  enum Flag {
    FLAG_NONE = 0,

    // Inline installs trigger slightly different behavior (install source
    // is different, download referrers are the item's page in the gallery).
    FLAG_INLINE_INSTALL = 1 << 0
  };

  class Delegate {
   public:
    virtual void OnExtensionInstallSuccess(const std::string& id) = 0;
    virtual void OnExtensionInstallFailure(const std::string& id,
                                           const std::string& error) = 0;
  };

  // Contains information about what parts of the extension install process can
  // be skipped or modified. If one of these is present, it means that a CRX
  // download was initiated by WebstoreInstaller. The Approval instance should
  // be checked further for additional details.
  struct Approval : public content::DownloadItem::ExternalData {
    static scoped_ptr<Approval> CreateWithInstallPrompt(Profile* profile);
    static scoped_ptr<Approval> CreateWithNoInstallPrompt(
        Profile* profile,
        const std::string& extension_id,
        scoped_ptr<base::DictionaryValue> parsed_manifest);

    virtual ~Approval();

    // The extension id that was approved for installation.
    std::string extension_id;

    // The profile the extension should be installed into.
    Profile* profile;

    // The expected manifest, before localization.
    scoped_ptr<base::DictionaryValue> parsed_manifest;

    // Whether to use a bubble notification when an app is installed, instead of
    // the default behavior of transitioning to the new tab page.
    bool use_app_installed_bubble;

    // Whether to skip the post install UI like the extension installed bubble.
    bool skip_post_install_ui;

    // Whether to skip the install dialog once the extension has been downloaded
    // and unpacked. One reason this can be true is that in the normal webstore
    // installation, the dialog is shown earlier, before any download is done,
    // so there's no need to show it again.
    bool skip_install_dialog;

   private:
    Approval();
  };

  // Gets the Approval associated with the |download|, or NULL if there's none.
  // Note that the Approval is owned by |download|.
  static const Approval* GetAssociatedApproval(
      const content::DownloadItem& download);

  // Creates a WebstoreInstaller for downloading and installing the extension
  // with the given |id| from the Chrome Web Store. If |delegate| is not NULL,
  // it will be notified when the install succeeds or fails. The installer will
  // use the specified |controller| to download the extension. Only one
  // WebstoreInstaller can use a specific controller at any given time. This
  // also associates the |approval| with this install.
  // Note: the delegate should stay alive until being called back.
  WebstoreInstaller(Profile* profile,
                    Delegate* delegate,
                    content::NavigationController* controller,
                    const std::string& id,
                    scoped_ptr<Approval> approval,
                    int flags);

  // Starts downloading and installing the extension.
  void Start();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Instead of using the default download directory, use |directory| instead.
  // This does *not* transfer ownership of |directory|.
  static void SetDownloadDirectoryForTests(FilePath* directory);

 private:
  friend class base::RefCounted<WebstoreInstaller>;
  virtual ~WebstoreInstaller();

  // DownloadManager::DownloadUrl callback.
  void OnDownloadStarted(content::DownloadId id, net::Error error);

  // DownloadItem::Observer implementation:
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE;

  // Starts downloading the extension to |file_path|.
  void StartDownload(const FilePath& file_path);

  // Reports an install |error| to the delegate for the given extension if this
  // managed its installation. This also removes the associated PendingInstall.
  void ReportFailure(const std::string& error);

  // Reports a successful install to the delegate for the given extension if
  // this managed its installation. This also removes the associated
  // PendingInstall.
  void ReportSuccess();

  content::NotificationRegistrar registrar_;
  Profile* profile_;
  Delegate* delegate_;
  content::NavigationController* controller_;
  std::string id_;
  // The DownloadItem is owned by the DownloadManager and is valid from when
  // OnDownloadStarted is called (with no error) until the DownloadItem
  // transitions to state REMOVING.
  content::DownloadItem* download_item_;
  int flags_;
  scoped_ptr<Approval> approval_;
  GURL download_url_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
