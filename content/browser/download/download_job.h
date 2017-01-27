// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_interrupt_reasons.h"

namespace content {
class WebContents;

// Unknown download progress.
extern const int kDownloadProgressUnknown;

// Unknown download speed.
extern const int kDownloadSpeedUnknown;

// DownloadJob contains internal download logic.
// The owner of DownloadJob(e.g DownloadItemImpl) should implement
// DownloadJob::Manager to be informed with important changes from
// DownloadJob and modify its states accordingly.
class CONTENT_EXPORT DownloadJob {
 public:
  // The interface used to interact with the owner of the download job.
  class CONTENT_EXPORT Manager {
   public:
    // Called when |download_job| becomes actively download data.
    virtual void OnSavingStarted(DownloadJob* download_job) = 0;

    // Called when |download_job| is interrupted.
    virtual void OnDownloadInterrupted(DownloadJob* download_job,
                                       DownloadInterruptReason reason) = 0;

    // Called when |download_job| is completed and inform the manager.
    virtual void OnDownloadComplete(DownloadJob* download_job) = 0;

    // Sets the download danger type.
    virtual void SetDangerType(DownloadDangerType danger_type) = 0;
  };

  DownloadJob();
  virtual ~DownloadJob();

  DownloadJob::Manager* manager() { return manager_; }

  // Called when DownloadJob is attached to DownloadJob::Manager.
  // |manager| can be nullptr.
  virtual void OnAttached(DownloadJob::Manager* manager);

  // Called before DownloadJob is detached from its manager.
  virtual void OnBeforeDetach();

  // Download operations.
  virtual void Cancel(bool user_cancel) = 0;
  virtual void Pause() = 0;
  virtual void Resume() = 0;

  // Return if the download file can be opened.
  // See |DownloadItem::CanOpenDownload|.
  virtual bool CanOpen() const = 0;

  // Return if the download job can be resumed.
  virtual bool CanResume() const = 0;

  // See |DownloadItem::CanShowInFolder|.
  virtual bool CanShowInFolder() const = 0;

  // Return if the download job is actively downloading.
  virtual bool IsActive() const = 0;

  // Return if the download job is paused.
  virtual bool IsPaused() const = 0;

  // Return a number between 0 and 100 to represent the percentage progress of
  // the download. Return |kDownloadProgressUnknown| if the progress is
  // indeterminate.
  virtual int PercentComplete() const = 0;

  // Return the estimated current download speed in bytes per second.
  // Or return kDownloadSpeedUnknown if the download speed is not measurable.
  virtual int64_t CurrentSpeed() const = 0;

  // Set the estimated remaining time of the download to |remaining| and return
  // true if time remaining is available, otherwise return false.
  virtual bool TimeRemaining(base::TimeDelta* remaining) const = 0;

  // Return the WebContents associated with the download. Usually used to
  // associate a browser window for any UI that needs to be displayed to the
  // user.
  // Or return nullptr if the download is not associated with an active
  // WebContents.
  virtual WebContents* GetWebContents() const = 0;

  // Print the debug string including internal states of the download job.
  virtual std::string DebugString(bool verbose) const = 0;

 protected:
  // Mark the download as being active.
  void StartedSavingResponse();

  // Mark the download as interrupted.
  void Interrupt(DownloadInterruptReason reason);

  // Mark the download job as completed.
  void Complete();

  // Owner of this DownloadJob, Only valid between OnAttached() and
  // OnBeforeDetach(), inclusive. Otherwise set to nullptr.
  DownloadJob::Manager* manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_H_
