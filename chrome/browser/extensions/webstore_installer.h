// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class GURL;
class Profile;

// Downloads and installs extensions from the web store.
class WebstoreInstaller : public NotificationObserver {
 public:
  enum Flag {
    FLAG_NONE = 0,

    // Sets the download's referral to the extension's page in the gallery.
    FLAG_OVERRIDE_REFERRER = 1 << 0
  };

  class Delegate {
   public:
    virtual void OnExtensionInstallSuccess(const std::string& id) = 0;
    virtual void OnExtensionInstallFailure(const std::string& id,
                                           const std::string& error) = 0;
  };

  explicit WebstoreInstaller(Profile* profile);
  virtual ~WebstoreInstaller();

  // Download and install the extension with the given |id| from the Chrome
  // Web Store. If |delegate| is not NULL, it will be notified when the
  // install succeeds or fails.
  // Note: the delegate should stay alive until being called back.
  void InstallExtension(const std::string& id, Delegate* delegate, int flags);

  // NotificationObserver
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  struct PendingInstall;

  // Removes the PendingIntall data for the given extension, returning true and
  // setting |install| if there is such data.
  bool ClearPendingInstall(const std::string& id, PendingInstall* install);

  // Creates the PendingInstall for the specified extension.
  const PendingInstall& CreatePendingInstall(
      const std::string& id, Delegate* delegate);

  // Gets the extension id for the given gallery install |url|.
  std::string GetPendingInstallId(const GURL& url);

  // Reports an install |error| to the delegate for the given extension if this
  // managed its installation. This also removes the associated PendingInstall.
  void ReportFailure(const std::string& id, const std::string& error);

  // Reports a successful install to the delegate for the given extension if
  // this managed its installation. This also removes the associated
  // PendingInstall.
  void ReportSuccess(const std::string& id);

  NotificationRegistrar registrar_;
  Profile* profile_;
  std::vector<PendingInstall> pending_installs_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
