// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mhtml_generation_manager.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/common/view_messages.h"

namespace content {

// The class and all of its members live on the UI thread.  Only static methods
// are executed on other threads.
class MHTMLGenerationManager::Job : public RenderProcessHostObserver {
 public:
  Job(int job_id, WebContents* web_contents, GenerateMHTMLCallback callback);
  ~Job() override;

  void set_browser_file(base::File file) { browser_file_ = file.Pass(); }

  GenerateMHTMLCallback callback() const { return callback_; }

  // Sends IPC to the renderer, asking for MHTML generation.
  // Returns true if the message was sent successfully; false otherwise.
  bool SendToRenderView();

  // Close the file on the file thread and respond back on the UI thread with
  // file size.
  void CloseFile(base::Callback<void(int64 file_size)> callback);

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

 private:
  static int64 CloseFileOnFileThread(base::File file);

  // Id used to map renderer responses to jobs.
  // See also MHTMLGenerationManager::id_to_job_ map.
  int job_id_;

  // The handle to the file the MHTML is saved to for the browser process.
  base::File browser_file_;

  // The IDs mapping to a specific contents.
  int process_id_;
  int routing_id_;

  // The callback to call once generation is complete.
  GenerateMHTMLCallback callback_;

  // RAII helper for registering this Job as a RenderProcessHost observer.
  ScopedObserver<RenderProcessHost, MHTMLGenerationManager::Job>
      observed_renderer_process_host_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

MHTMLGenerationManager::Job::Job(int job_id,
                                 WebContents* web_contents,
                                 GenerateMHTMLCallback callback)
    : job_id_(job_id),
      process_id_(web_contents->GetRenderProcessHost()->GetID()),
      routing_id_(web_contents->GetRenderViewHost()->GetRoutingID()),
      callback_(callback),
      observed_renderer_process_host_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

MHTMLGenerationManager::Job::~Job() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

bool MHTMLGenerationManager::Job::SendToRenderView() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_file_.IsValid());

  RenderViewHost* rvh = RenderViewHost::FromID(process_id_, routing_id_);
  if (!rvh) {  // The contents went away.
    return false;
  }

  observed_renderer_process_host_.Add(rvh->GetMainFrame()->GetProcess());

  IPC::PlatformFileForTransit renderer_file = IPC::GetFileHandleForProcess(
      browser_file_.GetPlatformFile(), rvh->GetProcess()->GetHandle(),
      false);  // |close_source_handle|.
  rvh->Send(
      new ViewMsg_SavePageAsMHTML(rvh->GetRoutingID(), job_id_, renderer_file));
  return true;
}

void MHTMLGenerationManager::Job::RenderProcessExited(
    RenderProcessHost* host,
    base::TerminationStatus status,
    int exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MHTMLGenerationManager::GetInstance()->RenderProcessExited(this);
}

void MHTMLGenerationManager::Job::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observed_renderer_process_host_.Remove(host);
}

void MHTMLGenerationManager::Job::CloseFile(
    base::Callback<void(int64)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!browser_file_.IsValid()) {
    callback.Run(-1);
    return;
  }

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::Job::CloseFileOnFileThread,
                 base::Passed(browser_file_.Pass())),
      callback);
}

// static
int64 MHTMLGenerationManager::Job::CloseFileOnFileThread(base::File file) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(file.IsValid());
  int64 file_size = file.GetLength();
  file.Close();
  return file_size;
}

MHTMLGenerationManager* MHTMLGenerationManager::GetInstance() {
  return base::Singleton<MHTMLGenerationManager>::get();
}

MHTMLGenerationManager::MHTMLGenerationManager() : next_job_id_(0) {}

MHTMLGenerationManager::~MHTMLGenerationManager() {
  STLDeleteValues(&id_to_job_);
}

void MHTMLGenerationManager::SaveMHTML(WebContents* web_contents,
                                       const base::FilePath& file_path,
                                       const GenerateMHTMLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int job_id = NewJob(web_contents, callback);

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::CreateFile, file_path),
      base::Bind(&MHTMLGenerationManager::OnFileAvailable,
                 base::Unretained(this),  // Safe b/c |this| is a singleton.
                 job_id));
}

void MHTMLGenerationManager::OnSavedPageAsMHTML(
    int job_id,
    bool mhtml_generation_in_renderer_succeeded) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JobStatus job_status = mhtml_generation_in_renderer_succeeded
                             ? JobStatus::SUCCESS
                             : JobStatus::FAILURE;
  JobFinished(job_id, job_status);
}

// static
base::File MHTMLGenerationManager::CreateFile(const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::File browser_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!browser_file.IsValid()) {
    LOG(ERROR) << "Failed to create file to save MHTML at: " <<
        file_path.value();
  }
  return browser_file.Pass();
}

void MHTMLGenerationManager::OnFileAvailable(int job_id,
                                             base::File browser_file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!browser_file.IsValid()) {
    LOG(ERROR) << "Failed to create file";
    JobFinished(job_id, JobStatus::FAILURE);
    return;
  }

  Job* job = FindJob(job_id);
  if (!job)
    return;

  job->set_browser_file(browser_file.Pass());

  if (!job->SendToRenderView()) {
    JobFinished(job_id, JobStatus::FAILURE);
  }
}

void MHTMLGenerationManager::JobFinished(int job_id, JobStatus job_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Job* job = FindJob(job_id);
  if (!job)
    return;

  job->CloseFile(
      base::Bind(&MHTMLGenerationManager::OnFileClosed,
                 base::Unretained(this),  // Safe b/c |this| is a singleton.
                 job_id, job_status));
}

void MHTMLGenerationManager::OnFileClosed(int job_id,
                                          JobStatus job_status,
                                          int64 file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Job* job = FindJob(job_id);
  if (!job)
    return;

  job->callback().Run(job_status == JobStatus::SUCCESS ? file_size : -1);
  id_to_job_.erase(job_id);
  delete job;
}

int MHTMLGenerationManager::NewJob(WebContents* web_contents,
                                   const GenerateMHTMLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int job_id = next_job_id_++;
  Job* job = new Job(job_id, web_contents, callback);
  id_to_job_[job_id] = job;
  return job_id;
}

MHTMLGenerationManager::Job* MHTMLGenerationManager::FindJob(int job_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  IDToJobMap::iterator iter = id_to_job_.find(job_id);
  if (iter == id_to_job_.end()) {
    NOTREACHED();
    return nullptr;
  }
  return iter->second;
}

void MHTMLGenerationManager::RenderProcessExited(Job* job) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (IDToJobMap::iterator it = id_to_job_.begin(); it != id_to_job_.end();
       ++it) {
    if (it->second == job) {
      JobFinished(it->first, JobStatus::FAILURE);
      return;
    }
  }
  NOTREACHED();
}

}  // namespace content
