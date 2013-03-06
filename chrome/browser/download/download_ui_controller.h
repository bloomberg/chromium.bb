// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_CONTROLLER_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/download/all_download_item_notifier.h"

class Profile;

namespace content {
class WebContents;
}

// This class handles the task of observing for download notifications and
// notifying the UI when a new download should be displayed in the UI.
//
// This class monitors each IN_PROGRESS download that is created (for which
// OnDownloadCreated is called) until:
// - it is assigned a target path or
// - is interrupted.
// Then the NotifyDownloadStarting() method of the Delegate is invoked.
class DownloadUIController : public AllDownloadItemNotifier::Observer {
 public:
  // The delegate is responsible for figuring out how to notify the UI.
  class Delegate {
   public:
    virtual ~Delegate();
    virtual void NotifyDownloadStarting(content::DownloadItem* item) = 0;
  };

  // |manager| is the download manager to observe for new downloads. If
  // |delegate.get()| is NULL, then the default delegate is constructed.
  //
  // On Android the default delegate notifies DownloadControllerAndroid. On
  // other platforms the target of the notification is a Browser object.
  //
  // Currently explicit delegates are only used for testing.
  DownloadUIController(content::DownloadManager* manager,
                       scoped_ptr<Delegate> delegate);

  virtual ~DownloadUIController();

 private:
  virtual void OnDownloadCreated(content::DownloadManager* manager,
                                 content::DownloadItem* item) OVERRIDE;
  virtual void OnDownloadUpdated(content::DownloadManager* manager,
                                 content::DownloadItem* item) OVERRIDE;

  AllDownloadItemNotifier download_notifier_;

  scoped_ptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUIController);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_CONTROLLER_H_
