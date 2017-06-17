// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_DOWNLOAD_DRIVER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_DOWNLOAD_DRIVER_H_

#include <set>
#include <string>

#include "base/optional.h"
#include "components/download/internal/driver_entry.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace base {
class FilePath;
}  // namespace base

namespace download {

struct RequestParams;

// The interface that includes all the operations to interact with low level
// download library functionalities.
class DownloadDriver {
 public:
  // The client to receive updates from content download library.
  // The update events for all downloads will pass through, so it's the
  // client's responsibility to filter the events it needs to handle.
  class Client {
   public:
    // Called when the low level download library is ready. |success| is true
    // when the low level download library is ready.
    virtual void OnDriverReady(bool success) = 0;

    // Called when any download is created.
    virtual void OnDownloadCreated(const DriverEntry& download) = 0;

    // Called when any download is failed. |reason| is propagated from low level
    // download library.
    virtual void OnDownloadFailed(const DriverEntry& download, int reason) = 0;

    // Called when any download is successfully completed.
    virtual void OnDownloadSucceeded(const DriverEntry& download,
                                     const base::FilePath& path) = 0;

    // Called when any download is updated.
    virtual void OnDownloadUpdated(const DriverEntry& download) = 0;
  };

  virtual ~DownloadDriver() = default;

  // Initialize the driver to receive download updates.
  virtual void Initialize(Client* client) = 0;

  // Returns if the driver is ready after the low level library has loaded all
  // the data. Returns false when the driver is not initialized by the client,
  // or low level download library has been shut down.
  virtual bool IsReady() const = 0;

  // Starts a new download.
  virtual void Start(
      const RequestParams& request_params,
      const std::string& guid,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;

  // Cancels an existing download, all data associated with this download should
  // be removed.
  virtual void Remove(const std::string& guid) = 0;

  // Pauses the download.
  virtual void Pause(const std::string& guid) = 0;

  // Resumes the download
  virtual void Resume(const std::string& guid) = 0;

  // Finds a download record from low level download library.
  virtual base::Optional<DriverEntry> Find(const std::string& guid) = 0;

  // Called to query the current set of active downloads.  This doesn't
  // necessarily mean downloads started by the service.
  virtual std::set<std::string> GetActiveDownloads() = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_DOWNLOAD_DRIVER_H_
