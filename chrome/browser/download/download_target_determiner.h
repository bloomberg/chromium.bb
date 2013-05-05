// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/download/download_path_reservation_tracker.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"

class ChromeDownloadManagerDelegate;
class Profile;
class DownloadPrefs;

namespace content {
enum DownloadDangerType;
}

// Determines the target of the download.
//
// Terminology:
//   Virtual Path: A path representing the target of the download that may or
//     may not be a physical file path. E.g. if the target of the download is in
//     cloud storage, then the virtual path may be relative to a logical mount
//     point.
//
//   Local Path: A local file system path where the downloads system should
//     write the file to.
//
//   Intermediate Path: Where the data should be written to during the course of
//     the download. Once the download completes, the file could be renamed to
//     Local Path.
//
// DownloadTargetDeterminer is a self owned object that performs the work of
// determining the download target. It observes the DownloadItem and aborts the
// process if the download is removed. DownloadTargetDeterminerDelegate is
// responsible for providing external dependencies and prompting the user if
// necessary.
//
// The only public entrypoint is the static Start() method which creates an
// instance of DownloadTargetDeterminer.
class DownloadTargetDeterminer
    : public content::DownloadItem::Observer {
 public:
  // Start the process of determing the target of |download|.
  //
  // |download_prefs| is required and must outlive |download|. It is used for
  //   determining the user's preferences regarding the default downloads
  //   directory, prompting and auto-open behavior.
  // |last_selected_directory| is the most recent directory that was chosen by
  //   the user. If the user needs to be prompted, then this directory will be
  //   used as the directory for the download instead of the user's default
  //   downloads directory.
  // |delegate| is required and must live until |callback| is invoked.
  // |callback| will be scheduled asynchronously on the UI thread after download
  //   determination is complete or after |download| is destroyed.
  //
  // Start() should be called on the UI thread.
  static void Start(content::DownloadItem* download,
                    DownloadPrefs* download_prefs,
                    const base::FilePath& last_selected_directory,
                    DownloadTargetDeterminerDelegate* delegate,
                    const content::DownloadTargetCallback& callback);

 private:
  // The main workflow is controlled via a set of state transitions. Each state
  // has an associated handler. The handler for STATE_FOO is DoFoo. Each handler
  // performs work, determines the next state to transition to and returns a
  // Result indicating how the workflow should proceed. The loop ends when a
  // handler returns COMPLETE.
  enum State {
    STATE_GENERATE_TARGET_PATH,
    STATE_NOTIFY_EXTENSIONS,
    STATE_RESERVE_VIRTUAL_PATH,
    STATE_PROMPT_USER_FOR_DOWNLOAD_PATH,
    STATE_DETERMINE_LOCAL_PATH,
    STATE_CHECK_DOWNLOAD_URL,
    STATE_CHECK_VISITED_REFERRER_BEFORE,
    STATE_DETERMINE_INTERMEDIATE_PATH,
    STATE_NONE,
  };

  // Result code returned by each step of the workflow below. Controls execution
  // of DoLoop().
  enum Result {
    // Continue processing. next_state_ is required to not be STATE_NONE.
    CONTINUE,

    // The DoLoop() that invoked the handler should exit. This value is
    // typically returned when the handler has invoked an asynchronous operation
    // and is expecting a callback. If a handler returns this value, it has
    // taken responsibility for ensuring that DoLoop() is invoked. It is
    // possible that the handler has invoked another DoLoop() already.
    QUIT_DOLOOP,

    // Target determination is complete.
    COMPLETE
  };

  // Used with IsDangerousFile to indicate whether the user has visited the
  // referrer URL for the download prior to today.
  enum PriorVisitsToReferrer {
    NO_VISITS_TO_REFERRER,
    VISITED_REFERRER,
  };

  // Construct a DownloadTargetDeterminer object. Constraints on the arguments
  // are as per Start() above.
  DownloadTargetDeterminer(
      content::DownloadItem* download,
      DownloadPrefs* download_prefs,
      const base::FilePath& last_selected_directory,
      DownloadTargetDeterminerDelegate* delegate,
      const content::DownloadTargetCallback& callback);

  virtual ~DownloadTargetDeterminer();

  // Invoke each successive handler until a handler returns QUIT_DOLOOP or
  // COMPLETE. Note that as a result, this object might be deleted. So |this|
  // should not be accessed after calling DoLoop().
  void DoLoop();

  // === Main workflow ===

  // Generates an initial target path. This target is based only on the state of
  // the download item.
  // Next state:
  // - STATE_NONE : If the download is not in progress, returns COMPLETE.
  // - STATE_NOTIFY_EXTENSIONS : All other downloads.
  Result DoGenerateTargetPath();

  // Notifies downloads extensions. If any extension wishes to override the
  // download filename, it will respond to the OnDeterminingFilename()
  // notification.
  // Next state:
  // - STATE_RESERVE_VIRTUAL_PATH.
  Result DoNotifyExtensions();

  // Callback invoked after extensions are notified. Updates |virtual_path_| and
  // |conflict_action_|.
  void NotifyExtensionsDone(
      const base::FilePath& new_path,
      DownloadPathReservationTracker::FilenameConflictAction conflict_action);

  // Invokes ReserveVirtualPath() on the delegate to acquire a reservation for
  // the path. See DownloadPathReservationTracker.
  // Next state:
  // - STATE_PROMPT_USER_FOR_DOWNLOAD_PATH.
  Result DoReserveVirtualPath();

  // Callback invoked after the delegate aquires a path reservation.
  void ReserveVirtualPathDone(const base::FilePath& path, bool verified);

  // Presents a file picker to the user if necessary.
  // Next state:
  // - STATE_DETERMINE_LOCAL_PATH.
  Result DoPromptUserForDownloadPath();

  // Callback invoked after the file picker completes. Cancels the download if
  // the user cancels the file picker.
  void PromptUserForDownloadPathDone(const base::FilePath& virtual_path);

  // Up until this point, the path that was used is considered to be a virtual
  // path. This step determines the local file system path corresponding to this
  // virtual path. The translation is done by invoking the DetermineLocalPath()
  // method on the delegate.
  // Next state:
  // - STATE_CHECK_DOWNLOAD_URL.
  Result DoDetermineLocalPath();

  // Callback invoked when the delegate has determined local path.
  void DetermineLocalPathDone(const base::FilePath& local_path);

  // Checks whether the downloaded URL is malicious. Invokes the
  // DownloadProtectionService via the delegate.
  // Next state:
  // - STATE_CHECK_VISITED_REFERRER_BEFORE.
  Result DoCheckDownloadUrl();

  // Callback invoked after the delegate has checked the download URL. Sets the
  // danger type of the download to |danger_type|.
  void CheckDownloadUrlDone(content::DownloadDangerType danger_type);

  // Checks if the user has visited the referrer URL of the download prior to
  // today. The actual check is only performed if it would be needed to
  // determine the danger type of the download.
  // Next state:
  // - STATE_DETERMINE_INTERMEDIATE_PATH.
  Result DoCheckVisitedReferrerBefore();

  // Callback invoked after completion of history check for prior visits to
  // referrer URL.
  void CheckVisitedReferrerBeforeDone(bool visited_referrer_before);

  // Determines the intermediate path. Once this step completes, downloads
  // target determination is complete. The determination assumes that the
  // intermediate file will never be overwritten (always uniquified if needed).
  // Next state:
  // - STATE_NONE: Returns COMPLETE.
  Result DoDetermineIntermediatePath();

  // === End of main workflow ===

  // Utilities:

  void ScheduleCallbackAndDeleteSelf();

  void CancelOnFailureAndDeleteSelf();

  Profile* GetProfile();

  bool ShouldPromptForDownload(const base::FilePath& filename);

  // Returns true if this download should show the "dangerous file" warning.
  // Various factors are considered, such as the type of the file, whether a
  // user action initiated the download, and whether the user has explicitly
  // marked the file type as "auto open". Protected virtual for testing.
  bool IsDangerousFile(PriorVisitsToReferrer visits);

  // content::DownloadItem::Observer
  virtual void OnDownloadDestroyed(content::DownloadItem* download) OVERRIDE;

  // state
  State next_state_;
  bool should_prompt_;
  bool create_directory_;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action_;
  content::DownloadDangerType danger_type_;
  base::FilePath virtual_path_;
  base::FilePath local_path_;
  base::FilePath intermediate_path_;

  content::DownloadItem* download_;
  DownloadPrefs* download_prefs_;
  DownloadTargetDeterminerDelegate* delegate_;
  base::FilePath last_selected_directory_;
  content::DownloadTargetCallback completion_callback_;
  CancelableRequestConsumer history_consumer_;

  base::WeakPtrFactory<DownloadTargetDeterminer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadTargetDeterminer);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_H_
