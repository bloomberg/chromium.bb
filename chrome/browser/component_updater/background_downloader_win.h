// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_BACKGROUND_DOWNLOADER_WIN_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_BACKGROUND_DOWNLOADER_WIN_H_

#include "chrome/browser/component_updater/crx_downloader.h"

#include <windows.h>
#include <bits.h>

#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "base/win/scoped_comptr.h"

namespace component_updater {

// Implements a downloader in terms of the BITS service. The public interface
// of this class and the CrxDownloader overrides are expected to be called
// from the UI thread. The rest of the class code runs on the FILE thread in
// a single threaded apartment.
class BackgroundDownloader : public CrxDownloader {
 protected:
  friend class CrxDownloader;
  BackgroundDownloader(scoped_ptr<CrxDownloader> successor,
                       net::URLRequestContextGetter* context_getter,
                       scoped_refptr<base::SequencedTaskRunner> task_runner,
                       const DownloadCallback& download_callback);
  virtual ~BackgroundDownloader();

 private:
  // Overrides for CrxDownloader.
  virtual void DoStartDownload(const GURL& url) OVERRIDE;

  // Called asynchronously on the FILE thread at different stages during
  // the download. |OnDownloading| can be called multiple times.
  void BeginDownload(const GURL& url);
  void OnDownloading();
  void EndDownload(HRESULT hr);

  // Handles the job state transitions to a final state.
  void OnStateTransferred();
  void OnStateError();
  void OnStateCancelled();
  void OnStateAcknowledged();

  HRESULT QueueBitsJob(const GURL& url);
  HRESULT CreateOrOpenJob(const GURL& url);
  HRESULT InitializeNewJob(const GURL& url);

  static HRESULT CleanupStaleJobs(
    base::win::ScopedComPtr<IBackgroundCopyManager> bits_manager);

  net::URLRequestContextGetter* context_getter_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_ptr<base::RepeatingTimer<BackgroundDownloader> > timer_;

  base::win::ScopedComPtr<IBackgroundCopyManager> bits_manager_;
  base::win::ScopedComPtr<IBackgroundCopyJob> job_;

  bool is_completed_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundDownloader);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_BACKGROUND_DOWNLOADER_WIN_H_

