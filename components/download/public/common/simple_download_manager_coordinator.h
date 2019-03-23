// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_url_parameters.h"

namespace download {

class DownloadItem;
class SimpleDownloadManager;

// This object allows swapping between different SimppleDownloadManager
// instances so that callers don't need to know about the swap.
class COMPONENTS_DOWNLOAD_EXPORT SimpleDownloadManagerCoordinator {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    virtual void OnDownloadsInitialized(bool active_downloads_only) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  SimpleDownloadManagerCoordinator();
  ~SimpleDownloadManagerCoordinator();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetSimpleDownloadManager(SimpleDownloadManager* simple_download_manager,
                                bool has_all_history_downloads);

  // See download::DownloadUrlParameters for details about controlling the
  // download.
  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> parameters);

  // Gets all the downloads. Caller needs to call has_all_history_downloads() to
  // check if all downloads are initialized. If only active downloads are
  // initialized, this method will only return all active downloads.
  void GetAllDownloads(std::vector<DownloadItem*>* downloads);

  // Get the download item for |guid|.
  DownloadItem* GetDownloadByGuid(const std::string& guid);

  bool has_all_history_downloads() const { return has_all_history_downloads_; }

 private:
  // Called when |simple_download_manager_| is initialized.
  void OnManagerInitialized(bool has_all_history_downloads);

  SimpleDownloadManager* simple_download_manager_;

  // Whether all the history downloads are ready.
  bool has_all_history_downloads_;

  // Whether this object is initialized and active downloads are ready to be
  // retrieved.
  bool initialized_;

  // Observers that want to be notified of changes to the set of downloads.
  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<SimpleDownloadManagerCoordinator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SimpleDownloadManagerCoordinator);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_H_
