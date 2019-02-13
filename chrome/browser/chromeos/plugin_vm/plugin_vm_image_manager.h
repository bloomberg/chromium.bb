// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/zlib/google/zip_reader.h"

namespace base {
template <typename T>
class Optional;
}  // namespace base

namespace download {
class DownloadService;
}  // namespace download

class Profile;

namespace plugin_vm {

constexpr char kCrosvmDir[] = "crosvm";
constexpr char kPvmDir[] = "pvm";
constexpr char kPluginVmImageDir[] = "default";

// PluginVmImageManager is responsible for management of PluginVm image
// including downloading this image from url specified by user policy and
// unzipping downloaded image archive to the specified location.
//
// Only one PluginVm image at a time is allowed to be processed.
// IsProcessingImage() should be called to check whether any image is being
// processed at the moment.
class PluginVmImageManager : public KeyedService {
 public:
  // Observer class for the PluginVm image related events.
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnDownloadStarted() = 0;
    virtual void OnProgressUpdated(
        base::Optional<double> fraction_complete) = 0;
    virtual void OnDownloadCompleted() = 0;
    virtual void OnDownloadCancelled() = 0;
    // TODO(https://crbug.com/904851): Add failure reasons.
    virtual void OnDownloadFailed() = 0;
    virtual void OnUnzipped() = 0;
    virtual void OnUnzippingFailed() = 0;
  };

  explicit PluginVmImageManager(Profile* profile);

  // Returns true if manager is processing PluginVm image at the moment.
  bool IsProcessingImage();
  // Doesn't start to download PluginVm image and calls OnDownloadFailed() in
  // case IsProcessingImage() returns true otherwise starts processing PluginVm
  // image by downloading it.
  void StartDownload();
  // Cancels the download of PluginVm image finishing the image processing.
  // Downloaded PluginVm image archive is being deleted.
  void CancelDownload();

  // Should be called when download of PluginVm image has been completed
  // successfully. If called in other cases - unzipping is not started
  // and OnUnzipped(false /* success */) is called.
  void StartUnzipping();
  // Sets flag that indicates that unzipping is cancelled. This flag is further
  // checked in PluginVmImageWriterDelegate and in cases it is set to true
  // OnUnzipped(false /* success */) called.
  void CancelUnzipping();

  void SetObserver(Observer* observer);
  void RemoveObserver();

  // Called by PluginVmImageDownloadClient.
  void OnDownloadStarted();
  void OnProgressUpdated(base::Optional<double> fraction_complete);
  void OnDownloadCompleted(base::FilePath file_path);
  void OnDownloadCancelled();
  void OnDownloadFailed();

  // Finishes the processing of PluginVm image. Deletes downloaded PluginVm
  // image archive. In case |success| is false also deletes PluginVm image.
  void OnUnzipped(bool success);

  void SetDownloadServiceForTesting(
      download::DownloadService* download_service);
  void SetDownloadedPluginVmImageArchiveForTesting(
      base::FilePath downloaded_plugin_vm_image_archive);
  std::string GetCurrentDownloadGuidForTesting();

 private:
  Profile* profile_ = nullptr;
  Observer* observer_ = nullptr;
  download::DownloadService* download_service_ = nullptr;
  bool processing_image_ = false;
  bool unzipping_cancelled_ = false;
  std::string current_download_guid_;
  base::FilePath downloaded_plugin_vm_image_archive_;
  base::FilePath plugin_vm_image_dir_;

  class PluginVmImageWriterDelegate : public zip::WriterDelegate {
   public:
    explicit PluginVmImageWriterDelegate(
        PluginVmImageManager* manager,
        const base::FilePath& output_file_path);

    // zip::WriterDelegate implementation.
    bool PrepareOutput() override;
    // If unzipping is cancelled returns false so unzipping would fail.
    bool WriteBytes(const char* data, int num_bytes) override;
    void SetTimeModified(const base::Time& time) override;

   private:
    PluginVmImageManager* manager_;
    base::FilePath output_file_path_;
    base::File output_file_;

    DISALLOW_COPY_AND_ASSIGN(PluginVmImageWriterDelegate);
  };

  ~PluginVmImageManager() override;

  GURL GetPluginVmImageDownloadUrl();
  download::DownloadParams GetDownloadParams(const GURL& url);

  void OnStartDownload(const std::string& download_guid,
                       download::DownloadParams::StartResult start_result);
  bool IsDownloading();
  // Returns true in case downloaded PluginVm image archive passes verification
  // and false otherwise.
  bool VerifyDownload(std::string downloaded_archive_hash);

  bool UnzipDownloadedPluginVmImageArchive();
  bool IsUnzippingCancelled();
  // Callback arguments for unzipping function.
  std::unique_ptr<zip::WriterDelegate> CreatePluginVmImageWriterDelegate(
      const base::FilePath& entry_path);
  bool CreateDirectory(const base::FilePath& entry_path);
  bool FilterFilesInPluginVmImageArchive(const base::FilePath& file);

  bool EnsureDownloadedPluginVmImageArchiveIsPresent();
  // Creates directory for PluginVm image if one doesn't exists.
  // Returns true in case directory has already existed or was successfully
  // created and false otherwise.
  bool EnsureDirectoryForPluginVmImageIsPresent();
  void RemoveTemporaryPluginVmImageArchiveIfExists();
  void OnTemporaryPluginVmImageArchiveRemoved(bool success);
  void RemovePluginVmImageDirectoryIfExists();
  void OnPluginVmImageDirectoryRemoved(bool success);

  base::WeakPtrFactory<PluginVmImageManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginVmImageManager);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_
