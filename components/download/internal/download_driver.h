// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_DOWNLOAD_DRIVER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_DOWNLOAD_DRIVER_H_

#include <string>

#include "base/optional.h"
#include "components/download/internal/driver_entry.h"

namespace base {
class FilePath;
}  // namespace base

namespace download {

struct DownloadParams;

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

  // Initialize the driver to receive download updates.
  virtual void Initialize(Client* client) = 0;

  // Returns if the driver is ready. Returns false when the driver is not
  // initialized by the client, or low level download library has been shut
  // down.
  virtual bool IsReady() const = 0;

  // Starts a new download.
  virtual void Start(const DownloadParams& params) = 0;

  // Cancels an existing download, all data associated with this download should
  // be removed.
  virtual void Cancel(const std::string& guid) = 0;

  // Pauses the download.
  virtual void Pause(const std::string& guid) = 0;

  // Resumes the download
  virtual void Resume(const std::string& guid) = 0;

  // Find a download record from low level download library.
  virtual base::Optional<DriverEntry> Find(const std::string& guid) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_DOWNLOAD_DRIVER_H_
