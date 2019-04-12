// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/zlib/google/zip_reader.h"

namespace download {
class DownloadService;
struct CompletionInfo;
}  // namespace download

class Profile;

namespace plugin_vm {

constexpr char kCrosvmDir[] = "crosvm";
constexpr char kPvmDir[] = "pvm";
constexpr char kPluginVmImageDir[] = "default";

// PluginVmImageManager is responsible for management of PluginVm image
// including downloading this image from url specified by the user policy,
// unzipping downloaded image archive to the specified location and registering
// final image.
//
// Only one PluginVm image at a time is allowed to be processed.
// Methods StartDownload(), StartUnzipping() and StartRegistration() should be
// called according to this order, image processing might be interrupted by
// calling according cancel methods. If one of the methods mentioned is called
// not in the correct order or before previous state is finished then associated
// fail method will be called by manager and image processing will be
// interrupted.
class PluginVmImageManager : public KeyedService {
 public:
  // Observer class for the PluginVm image related events.
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnDownloadStarted() = 0;
    virtual void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                           int64_t content_length,
                                           int64_t download_bytes_per_sec) = 0;
    virtual void OnDownloadCompleted() = 0;
    virtual void OnDownloadCancelled() = 0;
    // TODO(https://crbug.com/904851): Add failure reasons.
    virtual void OnDownloadFailed() = 0;
    virtual void OnUnzippingProgressUpdated(
        int64_t bytes_unzipped,
        int64_t plugin_vm_image_size,
        int64_t unzipping_bytes_per_sec) = 0;
    virtual void OnUnzipped() = 0;
    virtual void OnUnzippingFailed() = 0;
    virtual void OnRegistered() = 0;
    virtual void OnRegistrationFailed() = 0;
  };

  explicit PluginVmImageManager(Profile* profile);

  // Returns true if manager is processing PluginVm image at the moment.
  bool IsProcessingImage();
  void StartDownload();
  // Cancels the download of PluginVm image finishing the image processing.
  // Downloaded PluginVm image archive is being deleted.
  void CancelDownload();

  // Should be called when download of PluginVm image is successfully completed.
  // If called in other cases - unzipping is not started and
  // OnUnzipped(false /* success */) is called.
  void StartUnzipping();
  // Sets flag that indicates that unzipping is cancelled. This flag is further
  // checked in PluginVmImageWriterDelegate and in cases it is set to true
  // OnUnzipped(false /* success */) called to interrupt unzipping and
  // remove PluginVm image.
  void CancelUnzipping();

  // Should be called when download and unzipping of PluginVm are successfully
  // completed. If called in other cases - registration is not started and
  // OnRegistered(false /* success */) is called.
  void StartRegistration();
  // Sets flag that indicates that registration is cancelled. This flag is
  // further checked when OnRegistered(bool success) is called.
  void CancelRegistration();

  void SetObserver(Observer* observer);
  void RemoveObserver();

  // Called by PluginVmImageDownloadClient, are not supposed to be used by other
  // classes.
  void OnDownloadStarted();
  void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                 int64_t content_length);
  void OnDownloadCompleted(const download::CompletionInfo& info);
  void OnDownloadCancelled();
  void OnDownloadFailed();

  // Called by PluginVmImageWriterDelegate, are not supposed to be used by other
  // classes.
  void OnUnzippingProgressUpdated(int new_bytes_unzipped);
  // Deletes downloaded PluginVm image archive. In case |success| is false also
  // deletes PluginVm image.
  void OnUnzipped(bool success);

  // Returns true in case downloaded PluginVm image archive passes verification
  // and false otherwise. Public for testing purposes.
  bool VerifyDownload(const std::string& downloaded_archive_hash);

  void SetDownloadServiceForTesting(
      download::DownloadService* download_service);
  void SetDownloadedPluginVmImageArchiveForTesting(
      const base::FilePath& downloaded_plugin_vm_image_archive);
  std::string GetCurrentDownloadGuidForTesting();

 private:
  enum class State {
    NOT_STARTED,
    DOWNLOADING,
    DOWNLOAD_CANCELLED,
    DOWNLOADED,
    UNZIPPING,
    UNZIPPING_CANCELLED,
    UNZIPPED,
    REGISTERING,
    REGISTRATION_CANCELLED,
    REGISTERED,
    CONFIGURED,
    DOWNLOAD_FAILED,
    UNZIPPING_FAILED,
    REGISTRATION_FAILED,
  };

  Profile* profile_ = nullptr;
  Observer* observer_ = nullptr;
  download::DownloadService* download_service_ = nullptr;
  State state_ = State::NOT_STARTED;
  std::string current_download_guid_;
  base::FilePath downloaded_plugin_vm_image_archive_;
  base::FilePath plugin_vm_image_dir_;
  // -1 when is not yet determined.
  int64_t plugin_vm_image_size_ = -1;
  base::TimeTicks download_start_tick_;
  int64_t plugin_vm_image_bytes_unzipped_ = 0;
  base::TimeTicks unzipping_start_tick_;

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

  // Get string representation of state for logging purposes.
  std::string GetStateName(State state);

  GURL GetPluginVmImageDownloadUrl();
  download::DownloadParams GetDownloadParams(const GURL& url);

  void OnStartDownload(const std::string& download_guid,
                       download::DownloadParams::StartResult start_result);

  void CalculatePluginVmImageSize();
  bool UnzipDownloadedPluginVmImageArchive();
  bool IsUnzippingCancelled();
  // Callback arguments for unzipping function.
  std::unique_ptr<zip::WriterDelegate> CreatePluginVmImageWriterDelegate(
      const base::FilePath& entry_path);
  bool CreateDirectory(const base::FilePath& entry_path);
  bool FilterFilesInPluginVmImageArchive(const base::FilePath& file);

  // Finishes the processing of PluginVm image.
  void OnRegistered(bool success);

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
