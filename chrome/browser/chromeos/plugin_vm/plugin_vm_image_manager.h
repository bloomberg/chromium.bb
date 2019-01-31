// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
class Optional;
}  // namespace base

namespace download {
class DownloadService;
}  // namespace download

class Profile;

namespace plugin_vm {

// PluginVmImageManager is responsible for management of PluginVm image
// including downloading this image from url specified by user policy and
// unzipping downloaded image to the specified location.
//
// Only one download of PluginVm image at a time is allowed.
// IsDownloading() should be called to check whether the manager is downloading
// PluginVm image at the moment.
class PluginVmImageManager : public KeyedService {
 public:
  // Observer class for the PluginVm image related events.
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnDownloadStarted() = 0;
    virtual void OnProgressUpdated(
        base::Optional<double> fraction_complete) = 0;
    virtual void OnDownloadCompleted(base::FilePath file_path) = 0;
    virtual void OnDownloadCancelled() = 0;
    // TODO(https://crbug.com/904851): Add failure reasons.
    virtual void OnDownloadFailed() = 0;
  };

  explicit PluginVmImageManager(Profile* profile);

  // Returns true if manager is downloading PluginVm image at the moment.
  bool IsDownloading();
  // Fails download in case IsDownloading() returns true otherwise starts a new
  // download of PluginVm image.
  void StartDownload();
  // Cancels the download of PluginVm image.
  void CancelDownload();

  void SetObserver(Observer* observer);

  // Called by PluginVmImageDownloadClient.
  void OnDownloadStarted();
  void OnProgressUpdated(base::Optional<double> fraction_complete);
  void OnDownloadCompleted(base::FilePath file_path);
  void OnDownloadCancelled();
  void OnDownloadFailed();

  void SetDownloadServiceForTesting(
      download::DownloadService* download_service);
  std::string GetCurrentDownloadGuidForTesting();

 private:
  Profile* profile_ = nullptr;
  download::DownloadService* download_service_ = nullptr;
  Observer* observer_ = nullptr;
  bool processing_download_ = false;
  std::string current_download_guid_;

  ~PluginVmImageManager() override;

  download::DownloadParams GetDownloadParams();

  void OnStarted(const std::string& download_guid,
                 download::DownloadParams::StartResult start_result);

  DISALLOW_COPY_AND_ASSIGN(PluginVmImageManager);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_
