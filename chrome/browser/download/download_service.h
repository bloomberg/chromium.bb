// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class ChromeDownloadManagerDelegate;
class DownloadHistory;
class ExtensionDownloadsEventRouter;
class Profile;

namespace content {
class DownloadManager;
}

// Owning class for ChromeDownloadManagerDelegate.
class DownloadService : public ProfileKeyedService {
 public:
  explicit DownloadService(Profile* profile);
  virtual ~DownloadService();

  // Get the download manager delegate, creating it if it doesn't already exist.
  ChromeDownloadManagerDelegate* GetDownloadManagerDelegate();

  // Get the interface to the history system. Returns NULL if profile is
  // incognito or if the DownloadManager hasn't been created yet or if there is
  // no HistoryService for profile.
  DownloadHistory* GetDownloadHistory();

#if !defined(OS_ANDROID)
  ExtensionDownloadsEventRouter* GetExtensionEventRouter() {
    return extension_event_router_.get();
  }
#endif

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

  scoped_ptr<DownloadHistory> download_history_;

  // On Android, GET downloads are not handled by the DownloadManager.
  // Once we have extensions on android, we probably need the EventRouter
  // in ContentViewDownloadDelegate which knows about both GET and POST
  // downloads.
#if !defined(OS_ANDROID)
  // The ExtensionDownloadsEventRouter dispatches download creation, change, and
  // erase events to extensions. Like ChromeDownloadManagerDelegate, it's a
  // chrome-level concept and its lifetime should match DownloadManager. There
  // should be a separate EDER for on-record and off-record managers.
  // There does not appear to be a separate ExtensionSystem for on-record and
  // off-record profiles, so ExtensionSystem cannot own the EDER.
  scoped_ptr<ExtensionDownloadsEventRouter> extension_event_router_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DownloadService);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_H_
