// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class FilePath;
class Profile;

namespace content {
class NavigationController;
}

// Downloads and installs extensions from the web store.
class WebstoreInstaller : public content::NotificationObserver,
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


  // Creates a WebstoreInstaller for downloading and installing the extension
  // with the given |id| from the Chrome Web Store. If |delegate| is not NULL,
  // it will be notified when the install succeeds or fails. The installer will
  // use the specified |controller| to download the extension. Only one
  // WebstoreInstaller can use a specific controller at any given time.
  // Note: the delegate should stay alive until being called back.
  WebstoreInstaller(Profile* profile,
                    Delegate* delegate,
                    content::NavigationController* controller,
                    const std::string& id,
                    int flags);
  virtual ~WebstoreInstaller();

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
  // Starts downloading the extension to |file_path|.
  void StartDownload(FilePath file_path);

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
  int flags_;
  GURL download_url_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
