// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mhtml_generation_manager.h"

#include "base/bind.h"
#include "base/platform_file.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/common/view_messages.h"

namespace content {

class MHTMLGenerationManager::Job : public RenderProcessHostObserver {
 public:
  Job();
  virtual ~Job();

  void SetWebContents(WebContents* web_contents);

  base::PlatformFile browser_file() { return browser_file_; }
  void set_browser_file(base::PlatformFile file) { browser_file_ = file; }

  int process_id() { return process_id_; }
  int routing_id() { return routing_id_; }

  GenerateMHTMLCallback callback() { return callback_; }
  void set_callback(GenerateMHTMLCallback callback) { callback_ = callback; }

  // RenderProcessHostObserver:
  virtual void RenderProcessExited(RenderProcessHost* host,
                                   base::ProcessHandle handle,
                                   base::TerminationStatus status,
                                   int exit_code) OVERRIDE;
  virtual void RenderProcessHostDestroyed(RenderProcessHost* host) OVERRIDE;


 private:
  // The handle to the file the MHTML is saved to for the browser process.
  base::PlatformFile browser_file_;

  // The IDs mapping to a specific contents.
  int process_id_;
  int routing_id_;

  // The callback to call once generation is complete.
  GenerateMHTMLCallback callback_;

  // The RenderProcessHost being observed, or NULL if none is.
  RenderProcessHost* host_;
};

MHTMLGenerationManager::Job::Job()
    : browser_file_(base::kInvalidPlatformFileValue),
      process_id_(-1),
      routing_id_(-1),
      host_(NULL) {
}

MHTMLGenerationManager::Job::~Job() {
  if (host_)
    host_->RemoveObserver(this);
}

void MHTMLGenerationManager::Job::SetWebContents(WebContents* web_contents) {
  process_id_ = web_contents->GetRenderProcessHost()->GetID();
  routing_id_ = web_contents->GetRenderViewHost()->GetRoutingID();
  host_ = web_contents->GetRenderProcessHost();
  host_->AddObserver(this);
}

void MHTMLGenerationManager::Job::RenderProcessExited(
    RenderProcessHost* host,
    base::ProcessHandle handle,
    base::TerminationStatus status,
    int exit_code) {
  MHTMLGenerationManager::GetInstance()->RenderProcessExited(this);
}

void MHTMLGenerationManager::Job::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  host_ = NULL;
}

MHTMLGenerationManager* MHTMLGenerationManager::GetInstance() {
  return Singleton<MHTMLGenerationManager>::get();
}

MHTMLGenerationManager::MHTMLGenerationManager() {
}

MHTMLGenerationManager::~MHTMLGenerationManager() {
}

void MHTMLGenerationManager::SaveMHTML(WebContents* web_contents,
                                       const base::FilePath& file,
                                       const GenerateMHTMLCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int job_id = NewJob(web_contents, callback);

  base::ProcessHandle renderer_process =
      web_contents->GetRenderProcessHost()->GetHandle();
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::CreateFile, base::Unretained(this),
                 job_id, file, renderer_process));
}

void MHTMLGenerationManager::StreamMHTML(
    WebContents* web_contents,
    const base::PlatformFile browser_file,
    const GenerateMHTMLCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int job_id = NewJob(web_contents, callback);

  base::ProcessHandle renderer_process =
      web_contents->GetRenderProcessHost()->GetHandle();
  IPC::PlatformFileForTransit renderer_file =
      IPC::GetFileHandleForProcess(browser_file, renderer_process, false);

  FileHandleAvailable(job_id, browser_file, renderer_file);
}


void MHTMLGenerationManager::MHTMLGenerated(int job_id, int64 mhtml_data_size) {
  JobFinished(job_id, mhtml_data_size);
}

void MHTMLGenerationManager::CreateFile(
    int job_id, const base::FilePath& file_path,
    base::ProcessHandle renderer_process) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::PlatformFile browser_file = base::CreatePlatformFile(file_path,
      base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE,
      NULL, NULL);
  if (browser_file == base::kInvalidPlatformFileValue) {
    LOG(ERROR) << "Failed to create file to save MHTML at: " <<
        file_path.value();
  }

  IPC::PlatformFileForTransit renderer_file =
      IPC::GetFileHandleForProcess(browser_file, renderer_process, false);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&MHTMLGenerationManager::FileHandleAvailable,
                 base::Unretained(this),
                 job_id,
                 browser_file,
                 renderer_file));
}

void MHTMLGenerationManager::FileHandleAvailable(
    int job_id,
    base::PlatformFile browser_file,
    IPC::PlatformFileForTransit renderer_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (browser_file == base::kInvalidPlatformFileValue) {
    LOG(ERROR) << "Failed to create file";
    JobFinished(job_id, -1);
    return;
  }

  IDToJobMap::iterator iter = id_to_job_.find(job_id);
  if (iter == id_to_job_.end()) {
    NOTREACHED();
    return;
  }

  Job& job = iter->second;
  job.set_browser_file(browser_file);

  RenderViewHost* rvh = RenderViewHost::FromID(
      job.process_id(), job.routing_id());
  if (!rvh) {
    // The contents went away.
    JobFinished(job_id, -1);
    return;
  }

  rvh->Send(new ViewMsg_SavePageAsMHTML(rvh->GetRoutingID(), job_id,
                                        renderer_file));
}

void MHTMLGenerationManager::JobFinished(int job_id, int64 file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  IDToJobMap::iterator iter = id_to_job_.find(job_id);
  if (iter == id_to_job_.end()) {
    NOTREACHED();
    return;
  }

  Job& job = iter->second;
  job.callback().Run(file_size);

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::CloseFile, base::Unretained(this),
                 job.browser_file()));

  id_to_job_.erase(job_id);
}

void MHTMLGenerationManager::CloseFile(base::PlatformFile file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::ClosePlatformFile(file);
}

int MHTMLGenerationManager::NewJob(WebContents* web_contents,
                                   const GenerateMHTMLCallback& callback) {
  static int id_counter = 0;
  int job_id = id_counter++;
  Job& job = id_to_job_[job_id];
  job.SetWebContents(web_contents);
  job.set_callback(callback);
  return job_id;
}

void MHTMLGenerationManager::RenderProcessExited(Job* job) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (IDToJobMap::iterator it = id_to_job_.begin(); it != id_to_job_.end();
       ++it) {
    if (&it->second == job) {
      JobFinished(it->first, -1);
      return;
    }
  }
  NOTREACHED();
}

}  // namespace content
