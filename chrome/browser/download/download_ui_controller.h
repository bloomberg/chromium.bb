// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_CONTROLLER_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/download/all_download_item_notifier.h"

// This class handles the task of observing a single DownloadManager for
// notifying the UI when a new download should be displayed in the UI.
// It invokes the OnNewDownloadReady() method of hte Delegate when the
// target path is available for a new download.
class DownloadUIController : public AllDownloadItemNotifier::Observer {
 public:
  // The delegate is responsible for figuring out how to notify the UI.
  class Delegate {
   public:
    virtual ~Delegate();

    // This method is invoked to notify the UI of the new download |item|. Note
    // that |item| may be in any state by the time this method is invoked.
    virtual void OnNewDownloadReady(content::DownloadItem* item) = 0;
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
