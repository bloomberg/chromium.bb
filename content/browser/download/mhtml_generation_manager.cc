// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mhtml_generation_manager.h"

#include "base/bind.h"
#include "base/platform_file.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_types.h"

using content::BrowserThread;

MHTMLGenerationManager::Job::Job()
    : browser_file(base::kInvalidPlatformFileValue),
      renderer_file(IPC::InvalidPlatformFileForTransit()),
      process_id(-1),
      routing_id(-1) {
}

MHTMLGenerationManager::MHTMLGenerationManager() {
}

MHTMLGenerationManager::~MHTMLGenerationManager() {
}

void MHTMLGenerationManager::GenerateMHTML(TabContents* tab_contents,
    const FilePath& file,
    const GenerateMHTMLCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static int id_counter = 0;

  int job_id = id_counter++;
  Job job;
  job.file_path = file;
  job.process_id = tab_contents->GetRenderProcessHost()->GetID();
  job.routing_id = tab_contents->render_view_host()->routing_id();
  job.callback = callback;
  id_to_job_[job_id] = job;

  base::ProcessHandle renderer_process =
      tab_contents->GetRenderProcessHost()->GetHandle();
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::CreateFile, this,
                 job_id, file, renderer_process));
}

void MHTMLGenerationManager::MHTMLGenerated(int job_id, int64 mhtml_data_size) {
  JobFinished(job_id, mhtml_data_size);
}

void MHTMLGenerationManager::CreateFile(int job_id, const FilePath& file_path,
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

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::FileCreated, this,
                 job_id, browser_file, renderer_file));
}

void MHTMLGenerationManager::FileCreated(int job_id,
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
  job.browser_file = browser_file;
  job.renderer_file = renderer_file;

  RenderViewHost* rvh = RenderViewHost::FromID(job.process_id, job.routing_id);
  if (!rvh) {
    // The tab went away.
    JobFinished(job_id, -1);
    return;
  }

  rvh->Send(new ViewMsg_SavePageAsMHTML(rvh->routing_id(), job_id,
                                        renderer_file));
}

void MHTMLGenerationManager::JobFinished(int job_id, int64 file_size) {
  IDToJobMap::iterator iter = id_to_job_.find(job_id);
  if (iter == id_to_job_.end()) {
    NOTREACHED();
    return;
  }

  Job& job = iter->second;
  job.callback.Run(job.file_path, file_size);

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::CloseFile, this, job.browser_file));

  id_to_job_.erase(job_id);
}

void MHTMLGenerationManager::CloseFile(base::PlatformFile file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::ClosePlatformFile(file);
}
