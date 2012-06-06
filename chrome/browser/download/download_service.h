// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class ChromeDownloadManagerDelegate;
class Profile;

namespace content {
class DownloadManager;
class DownloadManagerDelegate;
}

// Owning class for ChromeDownloadManagerDelegate.
class DownloadService : public ProfileKeyedService {
 public:
  explicit DownloadService(Profile* profile);
  virtual ~DownloadService();

  // Register a callback to be called whenever the DownloadManager is created.
  typedef base::Callback<void(content::DownloadManager*)>
      OnManagerCreatedCallback;
  void OnManagerCreated(const OnManagerCreatedCallback& cb);

  // Get the download manager delegate, creating it if it doesn't already exist.
  content::DownloadManagerDelegate* GetDownloadManagerDelegate();

  // Has a download manager been created?
  bool HasCreatedDownloadManager();

  // Number of downloads associated with this instance of the service.
  int DownloadCount() const;

  // Number of downloads associated with all profiles.
  static int DownloadCountAllProfiles();

  // Sets the DownloadManagerDelegate associated with this object and
  // its DownloadManager.  Takes ownership of |delegate|, and destroys
  // the previous delegate.  For testing.
  void SetDownloadManagerDelegateForTesting(
      ChromeDownloadManagerDelegate* delegate);

  // Will be called to release references on other services as part
  // of Profile shutdown.
  virtual void Shutdown() OVERRIDE;

 private:
  bool download_manager_created_;
  Profile* profile_;

  // ChromeDownloadManagerDelegate may be the target of callbacks from
  // the history service/DB thread and must be kept alive for those
  // callbacks.
  scoped_refptr<ChromeDownloadManagerDelegate> manager_delegate_;

  std::vector<OnManagerCreatedCallback> on_manager_created_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(DownloadService);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_H_
