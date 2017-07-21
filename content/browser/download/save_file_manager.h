// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Objects that handle file operations for saving files, on the file thread.
//
// The SaveFileManager owns a set of SaveFile objects, each of which connects
// with a SaveItem object which belongs to one SavePackage and runs on the file
// thread for saving data in order to avoid disk activity on either network IO
// thread or the UI thread. It coordinates the notifications from the network
// and UI.
//
// The SaveFileManager itself is a singleton object owned by the
// ResourceDispatcherHostImpl.
//
// The data sent to SaveFileManager have 2 sources, one is from
// ResourceDispatcherHostImpl, run in network IO thread, the all sub-resources
// and save-only-HTML pages will be got from network IO. The second is from
// render process, those html pages which are serialized from DOM will be
// composed in render process and encoded to its original encoding, then sent
// to UI loop in browser process, then UI loop will dispatch the data to
// SaveFileManager on the file thread. SaveFileManager will directly
// call SaveFile's method to persist data.
//
// A typical saving job operation involves multiple threads and sequences:
//
// Updating an in progress save file
// io_thread
//      |----> data from net   ---->|
//                                  |
//                                  |
//      |----> data from    ---->|  |
//      |      render process    |  |
// ui_thread                     |  |
//                   download_task_runner (writes to disk)
//                               |----> stats ---->|
//                                              ui_thread (feedback for user)
//
//
// Cancel operations perform the inverse order when triggered by a user action:
// ui_thread (user click)
//    |----> cancel command ---->|
//    |           |      download_task_runner (close file)
//    |           |---------------------> cancel command ---->|
//    |                                               io_thread (stops net IO
// ui_thread (user close contents)                               for saving)
//    |----> cancel command ---->|
//                            Render process(stop serializing DOM and sending
//                                           data)
//
//
// The SaveFileManager tracks saving requests, mapping from a save item id to
// the SavePackage for the contents where the saving job was initiated. In the
// event of a contents closure during saving, the SavePackage will notify the
// SaveFileManage to cancel all SaveFile jobs.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_MANAGER_H_

#include <stdint.h>

#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/download/save_types.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace net {
class IOBuffer;
}

namespace content {
class ResourceContext;
class SaveFile;
class SavePackage;
struct Referrer;

class CONTENT_EXPORT SaveFileManager
    : public base::RefCountedThreadSafe<SaveFileManager> {
 public:
   // Returns the singleton instance of the SaveFileManager.
  static SaveFileManager* Get();

  SaveFileManager();

  // Lifetime management.
  void Shutdown();

  // Save the specified URL.  Caller has to guarantee that |save_package| will
  // be alive until the call to RemoveSaveFile.  Called on the UI thread (and in
  // case of network downloads forwarded to the ResourceDispatcherHostImpl on
  // the IO thread).
  void SaveURL(SaveItemId save_item_id,
               const GURL& url,
               const Referrer& referrer,
               int render_process_host_id,
               int render_view_routing_id,
               int render_frame_routing_id,
               SaveFileCreateInfo::SaveFileSource save_source,
               const base::FilePath& file_full_path,
               ResourceContext* context,
               SavePackage* save_package);

  // Notifications sent from the IO thread and run on the file thread:
  void StartSave(SaveFileCreateInfo* info);
  void UpdateSaveProgress(SaveItemId save_item_id,
                          net::IOBuffer* data,
                          int size);
  void SaveFinished(SaveItemId save_item_id,
                    SavePackageId save_package_id,
                    bool is_success);

  // Notifications sent from the UI thread and run on the file thread.
  // Cancel a SaveFile instance which has specified save item id.
  void CancelSave(SaveItemId save_item_id);

  // Called on the UI thread to remove a save package from SaveFileManager's
  // tracking map.
  void RemoveSaveFile(SaveItemId save_item_id, SavePackage* package);

  // Helper function for deleting specified file.
  void DeleteDirectoryOrFile(const base::FilePath& full_path, bool is_dir);

  // Runs on file thread to save a file by copying from file system when
  // original url is using file scheme.
  void SaveLocalFile(const GURL& original_file_url,
                     SaveItemId save_item_id,
                     SavePackageId save_package_id);

  // Renames all the successfully saved files.
  void RenameAllFiles(const FinalNamesMap& final_names,
                      const base::FilePath& resource_dir,
                      int render_process_id,
                      int render_frame_routing_id,
                      SavePackageId save_package_id);

  // When the user cancels the saving, we need to remove all remaining saved
  // files of this page saving job from save_file_map_.
  void RemoveSavedFileFromFileMap(const std::vector<SaveItemId>& save_item_ids);

 private:
  friend class base::RefCountedThreadSafe<SaveFileManager>;

  ~SaveFileManager();

  // A cleanup helper that runs on the file thread.
  void OnShutdown();

  // Called only on UI thread to get the SavePackage for a contents's browser
  // context.
  static SavePackage* GetSavePackageFromRenderIds(int render_process_id,
                                                  int render_frame_routing_id);

  // Look up the SavePackage according to save item id.
  SavePackage* LookupPackage(SaveItemId save_item_id);

  // Called only on the file thread.
  // Look up one in-progress saving item according to save item id.
  SaveFile* LookupSaveFile(SaveItemId save_item_id);

  // Help function for sending notification of canceling specific request.
  void SendCancelRequest(SaveItemId save_item_id);

  // Notifications sent from the file thread and run on the UI thread.

  // Lookup the SaveManager for this WebContents' saving browser context and
  // inform it the saving job has been started.
  void OnStartSave(const SaveFileCreateInfo& info);
  // Update the SavePackage with the current state of a started saving job.
  // If the SavePackage for this saving job is gone, cancel the request.
  void OnUpdateSaveProgress(SaveItemId save_item_id,
                            int64_t bytes_so_far,
                            bool write_success);
  // Update the SavePackage with the finish state, and remove the request
  // tracking entries.
  void OnSaveFinished(SaveItemId save_item_id,
                      int64_t bytes_so_far,
                      bool is_success);
  // Notifies SavePackage that the whole page saving job is finished.
  void OnFinishSavePageJob(int render_process_id,
                           int render_frame_routing_id,
                           SavePackageId save_package_id);

  // Notifications sent from the UI thread and run on the file thread.

  // Deletes a specified file on the file thread.
  void OnDeleteDirectoryOrFile(const base::FilePath& full_path, bool is_dir);

  // Notifications sent from the UI thread and run on the IO thread

  // Initiates a request for URL to be saved.
  void OnSaveURL(const GURL& url,
                 const Referrer& referrer,
                 SaveItemId save_item_id,
                 SavePackageId save_package_id,
                 int render_process_host_id,
                 int render_view_routing_id,
                 int render_frame_routing_id,
                 ResourceContext* context);
  // Call ResourceDispatcherHostImpl's CancelRequest method to execute cancel
  // action in the IO thread.
  void ExecuteCancelSaveRequest(int render_process_id, int request_id);

  // A map from save_item_id into SaveFiles.
  std::unordered_map<SaveItemId, std::unique_ptr<SaveFile>, SaveItemId::Hasher>
      save_file_map_;

  // Tracks which SavePackage to send data to, called only on UI thread.
  // SavePackageMap maps save item ids to their SavePackage.
  std::unordered_map<SaveItemId, SavePackage*, SaveItemId::Hasher> packages_;

  DISALLOW_COPY_AND_ASSIGN(SaveFileManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_MANAGER_H_
