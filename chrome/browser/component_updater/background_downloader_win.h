// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_BACKGROUND_DOWNLOADER_WIN_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_BACKGROUND_DOWNLOADER_WIN_H_

#include <windows.h>
#include <bits.h>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/scoped_comptr.h"
#include "chrome/browser/component_updater/crx_downloader.h"

namespace base {
class FilePath;
}

namespace component_updater {

// Implements a downloader in terms of the BITS service. The public interface
// of this class and the CrxDownloader overrides are expected to be called
// from the UI thread. The rest of the class code runs on the FILE thread in
// a single threaded apartment. Instances of this class are created and
// destroyed in the UI thread. See the implementation of the class destructor
// for details regarding the clean up of resources acquired in this class.
class BackgroundDownloader : public CrxDownloader {
 protected:
  friend class CrxDownloader;
  BackgroundDownloader(scoped_ptr<CrxDownloader> successor,
                       net::URLRequestContextGetter* context_getter,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);
  virtual ~BackgroundDownloader();

 private:
  // Overrides for CrxDownloader.
  virtual void DoStartDownload(const GURL& url) OVERRIDE;

  // Called asynchronously on the FILE thread at different stages during
  // the download. |OnDownloading| can be called multiple times.
  // |EndDownload| switches the execution flow from the FILE to the UI thread.
  // Accessing any data members of this object on the FILE thread after
  // calling |EndDownload| is unsafe.
  void BeginDownload(const GURL& url);
  void OnDownloading();
  void EndDownload(HRESULT hr);

  // Handles the job state transitions to a final state.
  void OnStateTransferred();
  void OnStateError();
  void OnStateCancelled();
  void OnStateAcknowledged();

  // Handles the transition to a transient state where the job is in the
  // queue but not actively transferring data.
  void OnStateQueued();

  // Handles the job state transition to a transient, non-final error state.
  void OnStateTransientError();

  // Handles the job state corresponding to transferring data.
  void OnStateTransferring();

  HRESULT QueueBitsJob(const GURL& url);
  HRESULT CreateOrOpenJob(const GURL& url);
  HRESULT InitializeNewJob(const GURL& url);

  // Returns true if at the time of the call, it appears that the job
  // has not been making progress toward completion.
  bool IsStuck();

  // Makes the downloaded file available to the caller by renaming the
  // temporary file to its destination and removing it from the BITS queue.
  HRESULT CompleteJob();

  net::URLRequestContextGetter* context_getter_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The timer and the BITS interface pointers have thread affinity. These
  // members are initialized on the FILE thread and they must be destroyed
  // on the FILE thread.
  scoped_ptr<base::RepeatingTimer<BackgroundDownloader> > timer_;

  base::win::ScopedComPtr<IBackgroundCopyManager> bits_manager_;
  base::win::ScopedComPtr<IBackgroundCopyJob> job_;

  // Contains the time when the download of the current url has started.
  base::Time download_start_time_;

  // Contains the time when the BITS job is last seen making progress.
  base::Time job_stuck_begin_time_;

  // True if EndDownload was called.
  bool is_completed_;

  // Contains the path of the downloaded file if the download was successful.
  base::FilePath response_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundDownloader);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_BACKGROUND_DOWNLOADER_WIN_H_
