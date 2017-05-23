// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/save_page_type.h"

namespace content {

class BrowserContext;
class WebContents;

// Called by SavePackage when it creates a DownloadItem.
using SavePackageDownloadCreatedCallback = base::Callback<void(DownloadItem*)>;

// Will be called asynchronously with the results of the ChooseSavePath
// operation.  If the delegate wants notification of the download item created
// in response to this operation, the SavePackageDownloadCreatedCallback will be
// non-null.
using SavePackagePathPickedCallback =
    base::Callback<void(const base::FilePath&,
                        SavePageType,
                        const SavePackageDownloadCreatedCallback&)>;

// Called with the results of DetermineDownloadTarget().
//
// |target_path| should be set to a non-empty path which is taken to be the
//     final target path for the download. Any file already at this path will be
//     overwritten.
//
// |disposition| and |danger_type| are attributes associated with the download
//     item and can be accessed via the DownloadItem accessors.
//
// |intermediate_path| specifies the path to the intermediate file. The download
//     will be written to this path until all the bytes have been written. Upon
//     completion, the file will be renamed to |target_path|.
//     |intermediate_path| could be the same as |target_path|. Both paths must
//     be in the same directory.
//
// |interrupt_reason| should be set to DOWNLOAD_INTERRUPT_REASON_NONE in
//     order to proceed with the download. DOWNLOAD_INTERRUPT_REASON_USER_CANCEL
//     results in the download being marked cancelled. Any other value results
//     in the download being marked as interrupted. The other fields are only
//     considered valid if |interrupt_reason| is NONE.
using DownloadTargetCallback =
    base::Callback<void(const base::FilePath& target_path,
                        DownloadItem::TargetDisposition disposition,
                        DownloadDangerType danger_type,
                        const base::FilePath& intermediate_path,
                        DownloadInterruptReason interrupt_reason)>;

// Called when a download delayed by the delegate has completed.
using DownloadOpenDelayedCallback = base::Callback<void(bool)>;

// Called with the result of CheckForFileExistence().
using CheckForFileExistenceCallback = base::OnceCallback<void(bool result)>;

using DownloadIdCallback = base::Callback<void(uint32_t)>;

// Browser's download manager: manages all downloads and destination view.
class CONTENT_EXPORT DownloadManagerDelegate {
 public:
  // Lets the delegate know that the download manager is shutting down.
  virtual void Shutdown() {}

  // Runs |callback| with a new download id when possible, perhaps
  // synchronously.
  virtual void GetNextId(const DownloadIdCallback& callback);

  // Called to notify the delegate that a new download |item| requires a
  // download target to be determined. The delegate should return |true| if it
  // will determine the target information and will invoke |callback|. The
  // callback may be invoked directly (synchronously). If this function returns
  // |false|, the download manager will continue the download using a default
  // target path.
  //
  // The state of the |item| shouldn't be modified during the process of
  // filename determination save for external data (GetExternalData() /
  // SetExternalData()).
  //
  // If the download should be canceled, |callback| should be invoked with an
  // empty |target_path| argument.
  virtual bool DetermineDownloadTarget(DownloadItem* item,
                                       const DownloadTargetCallback& callback);

  // Tests if a file type should be opened automatically.
  virtual bool ShouldOpenFileBasedOnExtension(const base::FilePath& path);

  // Allows the delegate to delay completion of the download.  This function
  // will either return true (in which case the download may complete)
  // or will call the callback passed when the download is ready for
  // completion.  This routine may be called multiple times; once the callback
  // has been called or the function has returned true for a particular
  // download it should continue to return true for that download.
  virtual bool ShouldCompleteDownload(
      DownloadItem* item,
      const base::Closure& complete_callback);

  // Allows the delegate to override opening the download. If this function
  // returns false, the delegate needs to call callback when it's done
  // with the item, and is responsible for opening it.  This function is called
  // after the final rename, but before the download state is set to COMPLETED.
  virtual bool ShouldOpenDownload(DownloadItem* item,
                                  const DownloadOpenDelayedCallback& callback);

  // Returns true if we need to generate a binary hash for downloads.
  virtual bool GenerateFileHash();

  // Retrieve the directories to save html pages and downloads to.
  virtual void GetSaveDir(BrowserContext* browser_context,
                          base::FilePath* website_save_dir,
                          base::FilePath* download_save_dir,
                          bool* skip_dir_check) {}

  // Asks the user for the path to save a page. The delegate calls the callback
  // to give the answer.
  virtual void ChooseSavePath(
      WebContents* web_contents,
      const base::FilePath& suggested_path,
      const base::FilePath::StringType& default_extension,
      bool can_save_as_complete,
      const SavePackagePathPickedCallback& callback) {
  }

  // Sanitize a filename that's going to be used for saving a subresource of a
  // SavePackage.
  //
  // If the delegate does nothing, the default filename already populated in
  // |filename| will be used. Otherwise, the delegate can update |filename| to
  // the desired filename.
  //
  // |filename| contains a basename with an extension, but without a path. This
  // should be the case on return as well. I.e. |filename| cannot specify a
  // relative path.
  virtual void SanitizeSavePackageResourceName(base::FilePath* filename) {}

  // Opens the file associated with this download.
  virtual void OpenDownload(DownloadItem* download) {}

  // Shows the download via the OS shell.
  virtual void ShowDownloadInShell(DownloadItem* download) {}

  // Checks whether a downloaded file still exists.
  virtual void CheckForFileExistence(DownloadItem* download,
                                     CheckForFileExistenceCallback callback) {}

  // Return a GUID string used for identifying the application to the system AV
  // function for scanning downloaded files. If no GUID is provided or if the
  // provided GUID is invalid, then the appropriate quarantining will be
  // performed manually without passing the download to the system AV function.
  //
  // This GUID is only used on Windows.
  virtual std::string ApplicationClientIdForFileScanning() const;

 protected:
  virtual ~DownloadManagerDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
