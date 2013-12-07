// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/background_downloader_win.h"

#include <atlbase.h>
#include <atlcom.h>

#include <functional>
#include <iomanip>
#include <vector>

#include "base/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/win/scoped_co_mem.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/win/atl_module.h"
#include "url/gurl.h"

using base::win::ScopedCoMem;
using base::win::ScopedComPtr;
using content::BrowserThread;

// The class BackgroundDownloader in this module is an adapter between
// the CrxDownloader interface and the BITS service interfaces.
// The interface exposed on the CrxDownloader code runs on the UI thread, while
// the BITS specific code runs in a single threaded apartment on the FILE
// thread.
// For every url to download, a BITS job is created, unless there is already
// an existing job for that url, in which case, the downloader connects to it.
// Once a job is associated with the url, the code looks for changes in the
// BITS job state. The checks are triggered by a timer.
// The BITS job contains just one file to download. There could only be one
// download in progress at a time. If Chrome closes down before the download is
// complete, the BITS job remains active and finishes in the background, without
// any intervention. The job can be completed next time the code runs, if the
// file is still needed, otherwise it will be cleaned up on a periodic basis.
//
// To list the BITS jobs for a user, use the |bitsadmin| tool. The command line
// to do that is: "bitsadmin /list /verbose". Another useful command is
// "bitsadmin /info" and provide the job id returned by the previous /list
// command.
namespace component_updater {

namespace {

// All jobs created by this module have a specific description so they can
// be found at run-time or by using system administration tools.
const char16 kJobDescription[] = L"Chrome Component Updater";

// How often the code looks for changes in the BITS job state.
const int kJobPollingIntervalSec = 10;

// How often the jobs which were started but not completed for any reason
// are cleaned up. Reasons for jobs to be left behind include browser restarts,
// system restarts, etc. Also, the check to purge stale jobs only happens
// at most once a day.
const int kPurgeStaleJobsAfterDays = 7;
const int kPurgeStaleJobsIntervalBetweenChecksDays = 1;

// Returns the status code from a given BITS error.
int GetHttpStatusFromBitsError(HRESULT error) {
  // BITS errors are defined in bitsmsg.h. Although not documented, it is
  // clear that all errors corresponding to http status code have the high
  // word equal to 0x8019 and the low word equal to the http status code.
  const int kHttpStatusFirst = 100;    // Continue.
  const int kHttpStatusLast  = 505;    // Version not supported.
  bool is_valid = HIWORD(error) == 0x8019 &&
                  LOWORD(error) >= kHttpStatusFirst &&
                  LOWORD(error) <= kHttpStatusLast;
  return is_valid ? LOWORD(error) : 0;
}

// Returns the files in a BITS job.
HRESULT GetFilesInJob(IBackgroundCopyJob* job,
                      std::vector<ScopedComPtr<IBackgroundCopyFile> >* files) {
  ScopedComPtr<IEnumBackgroundCopyFiles> enum_files;
  HRESULT hr = job->EnumFiles(enum_files.Receive());
  if (FAILED(hr))
    return hr;

  ULONG num_files = 0;
  hr = enum_files->GetCount(&num_files);
  if (FAILED(hr))
    return hr;

  for (ULONG i = 0; i != num_files; ++i) {
    ScopedComPtr<IBackgroundCopyFile> file;
    if (enum_files->Next(1, file.Receive(), NULL) == S_OK)
      files->push_back(file);
  }

  return S_OK;
}

// Returns the file name, the url, and some per-file progress information.
// The function out parameters can be NULL if that data is not requested.
HRESULT GetJobFileProperties(IBackgroundCopyFile* file,
                             string16* local_name,
                             string16* remote_name,
                             BG_FILE_PROGRESS* progress) {
  HRESULT hr = S_OK;

  if (local_name) {
    ScopedCoMem<char16> name;
    hr = file->GetLocalName(&name);
    if (FAILED(hr))
      return hr;
    local_name->assign(name);
  }

  if (remote_name) {
    ScopedCoMem<char16> name;
    hr = file->GetRemoteName(&name);
    if (FAILED(hr))
      return hr;
    remote_name->assign(name);
  }

  if (progress) {
    BG_FILE_PROGRESS bg_file_progress = {};
    hr = file->GetProgress(&bg_file_progress);
    if (FAILED(hr))
      return hr;
    *progress = bg_file_progress;
  }

  return hr;
}

HRESULT GetJobDescription(IBackgroundCopyJob* job, const string16* name) {
  ScopedCoMem<char16> description;
  return job->GetDescription(&description);
}

// Finds the component updater jobs matching the given predicate.
// Returns S_OK if the function has found at least one job, returns S_FALSE if
// no job was found, and it returns an error otherwise.
template<class Predicate>
HRESULT FindBitsJobIf(Predicate pred,
                      IBackgroundCopyManager* bits_manager,
                      std::vector<ScopedComPtr<IBackgroundCopyJob> >* jobs) {
  ScopedComPtr<IEnumBackgroundCopyJobs> enum_jobs;
  HRESULT hr = bits_manager->EnumJobs(0, enum_jobs.Receive());
  if (FAILED(hr))
    return hr;

  ULONG job_count = 0;
  hr = enum_jobs->GetCount(&job_count);
  if (FAILED(hr))
    return hr;

  // Iterate over jobs, run the predicate, and select the job only if
  // the job description matches the component updater jobs.
  for (ULONG i = 0; i != job_count; ++i) {
    ScopedComPtr<IBackgroundCopyJob> current_job;
    if (enum_jobs->Next(1, current_job.Receive(), NULL) == S_OK &&
        pred(current_job)) {
      string16 job_description;
      hr = GetJobDescription(current_job, &job_description);
      if (job_description.compare(kJobDescription) == 0)
        jobs->push_back(current_job);
    }
  }

  return jobs->empty() ? S_FALSE : S_OK;
}

// Compares the job creation time and returns true if the job creation time
// is older than |num_days|.
struct JobCreationOlderThanDays
    : public std::binary_function<IBackgroundCopyJob*, int, bool> {
  bool operator()(IBackgroundCopyJob* job, int num_days) const;
};

bool JobCreationOlderThanDays::operator()(IBackgroundCopyJob* job,
                                          int num_days) const {
  BG_JOB_TIMES times = {0};
  HRESULT hr = job->GetTimes(&times);
  if (FAILED(hr))
    return false;

  const base::TimeDelta time_delta(base::TimeDelta::FromDays(num_days));
  const base::Time creation_time(base::Time::FromFileTime(times.CreationTime));

  return creation_time + time_delta < base::Time::Now();
}

// Compares the url of a file in a job and returns true if the remote name
// of any file in a job matches the argument.
struct JobFileUrlEqual
    : public std::binary_function<IBackgroundCopyJob*, const string16&, bool> {
  bool operator()(IBackgroundCopyJob* job, const string16& remote_name) const;
};

bool JobFileUrlEqual::operator()(IBackgroundCopyJob* job,
                                 const string16& remote_name) const {
  std::vector<ScopedComPtr<IBackgroundCopyFile> > files;
  HRESULT hr = GetFilesInJob(job, &files);
  if (FAILED(hr))
    return false;

  for (size_t i = 0; i != files.size(); ++i) {
    ScopedCoMem<char16> name;
    if (SUCCEEDED(files[i]->GetRemoteName(&name)) &&
        remote_name.compare(name) == 0)
      return true;
  }

  return false;
}

// Creates an instance of the BITS manager.
HRESULT GetBitsManager(IBackgroundCopyManager** bits_manager) {
  ScopedComPtr<IBackgroundCopyManager> object;
  HRESULT hr = object.CreateInstance(__uuidof(BackgroundCopyManager));
  if (FAILED(hr)) {
    VLOG(1) << "Failed to instantiate BITS." << std::hex << hr;
    // TODO: add UMA pings.
    return hr;
  }
  *bits_manager = object.Detach();
  return S_OK;
}

}  // namespace

BackgroundDownloader::BackgroundDownloader(
    scoped_ptr<CrxDownloader> successor,
    net::URLRequestContextGetter* context_getter,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const DownloadCallback& download_callback)
    : CrxDownloader(successor.Pass(), download_callback),
      context_getter_(context_getter),
      task_runner_(task_runner),
      is_completed_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

BackgroundDownloader::~BackgroundDownloader() {
}

void BackgroundDownloader::DoStartDownload(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&BackgroundDownloader::BeginDownload,
                   base::Unretained(this),
                   url));
}

// Called once when this class is asked to do a download. Creates or opens
// an existing bits job, hooks up the notifications, and starts the timer.
void BackgroundDownloader::BeginDownload(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DCHECK(!timer_);

  HRESULT hr = QueueBitsJob(url);
  if (FAILED(hr)) {
    if (job_) {
      job_->Cancel();
      job_ = NULL;
    }
    EndDownload(hr);
    return;
  }

  // A repeating timer retains the user task. This timer can be stopped and
  // reset multiple times.
  timer_.reset(new base::RepeatingTimer<BackgroundDownloader>);
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromSeconds(kJobPollingIntervalSec),
                this,
                &BackgroundDownloader::OnDownloading);
}

// Called any time the timer fires.
void BackgroundDownloader::OnDownloading() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  timer_->Stop();

  BG_JOB_STATE job_state = BG_JOB_STATE_ERROR;
  HRESULT hr = job_->GetState(&job_state);
  if (FAILED(hr)) {
    EndDownload(hr);
    return;
  }

  switch (job_state) {
    case BG_JOB_STATE_TRANSFERRED:
      OnStateTransferred();
      return;

    case BG_JOB_STATE_ERROR:
      OnStateError();
      return;

    case BG_JOB_STATE_CANCELLED:
      OnStateCancelled();
      return;

    case BG_JOB_STATE_ACKNOWLEDGED:
      OnStateAcknowledged();
      return;

    // TODO: handle the non-final states, so that the download does not get
    // stuck if BITS is not able to make progress on a given url.
    case BG_JOB_STATE_TRANSIENT_ERROR:
    case BG_JOB_STATE_QUEUED:
    case BG_JOB_STATE_CONNECTING:
    case BG_JOB_STATE_TRANSFERRING:
    case BG_JOB_STATE_SUSPENDED:
    default:
      break;
  }

  timer_->Reset();
}

// Completes the BITS download, picks up the file path of the response, and
// notifies the CrxDownloader. The function should be called only once.
void BackgroundDownloader::EndDownload(HRESULT error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DCHECK(!is_completed_);
  if (is_completed_)
    return;

  is_completed_ = true;

  DCHECK(!timer_->IsRunning());
  timer_.reset();

  base::FilePath response;
  HRESULT hr = error;
  if (SUCCEEDED(hr)) {
    std::vector<ScopedComPtr<IBackgroundCopyFile> > files;
    GetFilesInJob(job_, &files);
    DCHECK(files.size() == 1);
    string16 local_name;
    BG_FILE_PROGRESS progress = {0};
    hr = GetJobFileProperties(files[0], &local_name, NULL, &progress);
    if (SUCCEEDED(hr)) {
      DCHECK(progress.Completed);
      response = base::FilePath(local_name);
    }
  }

  // Consider the url handled if it has been successfully downloaded or a
  // 5xx has been received.
  const bool is_handled = SUCCEEDED(hr) ||
                          IsHttpServerError(GetHttpStatusFromBitsError(error));

  Result result;
  result.error = error;
  result.is_background_download = true;
  result.response = response;
  BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&BackgroundDownloader::OnDownloadComplete,
                   base::Unretained(this),
                   is_handled,
                   result));

  CleanupStaleJobs(bits_manager_);
}

// Called when the BITS job has been transferred successfully. Completes the
// BITS job by removing it from the BITS queue and making the download
// available to the caller.
void BackgroundDownloader::OnStateTransferred() {
  HRESULT hr = job_->Complete();
  if (SUCCEEDED(hr) || hr == BG_S_UNABLE_TO_DELETE_FILES)
    hr = S_OK;
  else
    job_->Cancel();

  EndDownload(hr);
}

// Called when the job has encountered an error and no further progress can
// be made. Cancels this job and removes it from the BITS queue.
void BackgroundDownloader::OnStateError() {
  ScopedComPtr<IBackgroundCopyError> copy_error;
  HRESULT hr = job_->GetError(copy_error.Receive());
  if (SUCCEEDED(hr)) {
    BG_ERROR_CONTEXT error_context = BG_ERROR_CONTEXT_NONE;
    HRESULT error_code = E_FAIL;
    hr = copy_error->GetError(&error_context, &error_code);
    if (SUCCEEDED(hr)) {
      EndDownload(error_code);
      return;
    }
  }

  job_->Cancel();
  EndDownload(hr);
}

// Called when the download was cancelled. Since the observer should have
// been disconnected by now, this notification must not be seen.
void BackgroundDownloader::OnStateCancelled() {
  EndDownload(E_UNEXPECTED);
}

// Called when the download was completed. Same as above.
void BackgroundDownloader::OnStateAcknowledged() {
  EndDownload(E_UNEXPECTED);
}

// Creates or opens a job for the given url and queues it up. Tries to
// install a job observer but continues on if an observer can't be set up.
HRESULT BackgroundDownloader::QueueBitsJob(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  HRESULT hr = S_OK;
  if (bits_manager_ == NULL) {
    hr = GetBitsManager(bits_manager_.Receive());
    if (FAILED(hr))
      return hr;
  }

  hr = CreateOrOpenJob(url);
  if (FAILED(hr))
    return hr;

  if (hr == S_OK) {
    hr = InitializeNewJob(url);
    if (FAILED(hr))
      return hr;
  }

  return job_->Resume();
}

HRESULT BackgroundDownloader::CreateOrOpenJob(const GURL& url) {
  std::vector<ScopedComPtr<IBackgroundCopyJob> > jobs;
  HRESULT hr = FindBitsJobIf(
      std::bind2nd(JobFileUrlEqual(), base::SysUTF8ToWide(url.spec())),
      bits_manager_,
      &jobs);
  if (SUCCEEDED(hr) && !jobs.empty()) {
    job_ = jobs.front();
    return S_FALSE;
  }

  // Use kJobDescription as a temporary job display name until the proper
  // display name is initialized later on.
  GUID guid = {0};
  ScopedComPtr<IBackgroundCopyJob> job;
  hr = bits_manager_->CreateJob(kJobDescription,
                                BG_JOB_TYPE_DOWNLOAD,
                                &guid,
                                job.Receive());
  if (FAILED(hr))
    return hr;

  job_ = job;
  return S_OK;
}

HRESULT BackgroundDownloader::InitializeNewJob(const GURL& url) {
  const string16 filename(base::SysUTF8ToWide(url.ExtractFileName()));

  base::FilePath tempdir;
  if (!base::CreateNewTempDirectory(
      FILE_PATH_LITERAL("chrome_BITS_"),
      &tempdir))
    return E_FAIL;

  HRESULT hr = job_->AddFile(
      base::SysUTF8ToWide(url.spec()).c_str(),
      tempdir.Append(filename).AsUTF16Unsafe().c_str());
  if (FAILED(hr))
    return hr;

  hr = job_->SetDisplayName(filename.c_str());
  if (FAILED(hr))
    return hr;

  hr = job_->SetDescription(kJobDescription);
  if (FAILED(hr))
    return hr;

  hr = job_->SetPriority(BG_JOB_PRIORITY_NORMAL);
  if (FAILED(hr))
    return hr;

  return S_OK;
}

// Cleans up incompleted jobs that are too old.
HRESULT BackgroundDownloader::CleanupStaleJobs(
    base::win::ScopedComPtr<IBackgroundCopyManager> bits_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!bits_manager)
    return E_FAIL;

  static base::Time last_sweep;

  const base::TimeDelta time_delta(base::TimeDelta::FromDays(
      kPurgeStaleJobsIntervalBetweenChecksDays));
  const base::Time current_time(base::Time::Now());
  if (last_sweep + time_delta > current_time)
    return S_OK;

  last_sweep = current_time;

  std::vector<ScopedComPtr<IBackgroundCopyJob> > jobs;
  HRESULT hr = FindBitsJobIf(
      std::bind2nd(JobCreationOlderThanDays(), kPurgeStaleJobsAfterDays),
      bits_manager,
      &jobs);
  if (FAILED(hr))
    return hr;

  for (size_t i = 0; i != jobs.size(); ++i) {
    jobs[i]->Cancel();
  }

  return S_OK;
}

}  // namespace component_updater

