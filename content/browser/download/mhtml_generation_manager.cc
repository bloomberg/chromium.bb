// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mhtml_generation_manager.h"

#include <map>
#include <queue>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"

namespace content {

// The class and all of its members live on the UI thread.  Only static methods
// are executed on other threads.
class MHTMLGenerationManager::Job : public RenderProcessHostObserver {
 public:
  Job(int job_id, WebContents* web_contents, GenerateMHTMLCallback callback);
  ~Job() override;

  void set_browser_file(base::File file) { browser_file_ = file.Pass(); }

  GenerateMHTMLCallback callback() const { return callback_; }

  // Sends IPC to the renderer, asking for MHTML generation of the next frame.
  //
  // Returns true if the message was sent successfully; false otherwise.
  bool SendToNextRenderFrame();

  // Indicates if more calls to SendToNextRenderFrame are needed.
  bool HasMoreFramesToProcess() const {
    return !pending_frame_tree_node_ids_.empty();
  }

  // Close the file on the file thread and respond back on the UI thread with
  // file size.
  void CloseFile(base::Callback<void(int64 file_size)> callback);

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

 private:
  static std::string GenerateMHTMLBoundaryMarker();
  static int64 CloseFileOnFileThread(base::File file);
  void AddFrame(RenderFrameHost* render_frame_host);

  // Creates a new map with values (content ids) the same as in
  // |frame_tree_node_to_content_id_| map, but with the keys translated from
  // frame_tree_node_id into a |site_instance|-specific routing_id.
  std::map<int, std::string> CreateFrameRoutingIdToContentId(
      SiteInstance* site_instance);

  // Id used to map renderer responses to jobs.
  // See also MHTMLGenerationManager::id_to_job_ map.
  int job_id_;

  // The handle to the file the MHTML is saved to for the browser process.
  base::File browser_file_;

  // The IDs of frames we still need to process.
  std::queue<int> pending_frame_tree_node_ids_;

  // Map from frames into content ids (see WebPageSerializer::generateMHTMLParts
  // for more details about what "content ids" are and how they are used).
  std::map<int, std::string> frame_tree_node_to_content_id_;

  // MIME multipart boundary to use in the MHTML doc.
  std::string mhtml_boundary_marker_;

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
      mhtml_boundary_marker_(GenerateMHTMLBoundaryMarker()),
      callback_(callback),
      observed_renderer_process_host_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents->ForEachFrame(base::Bind(
      &MHTMLGenerationManager::Job::AddFrame,
      base::Unretained(this)));  // Safe because ForEachFrame is synchronous.

  // Main frame needs to be processed first.
  DCHECK(!pending_frame_tree_node_ids_.empty());
  DCHECK(FrameTreeNode::GloballyFindByID(pending_frame_tree_node_ids_.front())
             ->parent() == nullptr);
}

MHTMLGenerationManager::Job::~Job() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

std::map<int, std::string>
MHTMLGenerationManager::Job::CreateFrameRoutingIdToContentId(
    SiteInstance* site_instance) {
  std::map<int, std::string> result;
  for (const auto& it : frame_tree_node_to_content_id_) {
    int ftn_id = it.first;
    const std::string& content_id = it.second;

    FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(ftn_id);
    if (!ftn)
      continue;

    int routing_id =
        ftn->render_manager()->GetRoutingIdForSiteInstance(site_instance);
    if (routing_id == MSG_ROUTING_NONE)
      continue;

    result[routing_id] = content_id;
  }
  return result;
}

bool MHTMLGenerationManager::Job::SendToNextRenderFrame() {
  DCHECK(browser_file_.IsValid());
  DCHECK_LT(0u, pending_frame_tree_node_ids_.size());

  int frame_tree_node_id = pending_frame_tree_node_ids_.front();
  pending_frame_tree_node_ids_.pop();
  bool is_last_frame = pending_frame_tree_node_ids_.empty();

  FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!ftn)  // The contents went away.
    return false;
  RenderFrameHost* rfh = ftn->current_frame_host();

  // Get notified if the target of the IPC message dies between responding.
  observed_renderer_process_host_.RemoveAll();
  observed_renderer_process_host_.Add(rfh->GetProcess());

  IPC::PlatformFileForTransit renderer_file = IPC::GetFileHandleForProcess(
      browser_file_.GetPlatformFile(), rfh->GetProcess()->GetHandle(),
      false);  // |close_source_handle|.
  rfh->Send(new FrameMsg_SerializeAsMHTML(
      rfh->GetRoutingID(), job_id_, renderer_file, mhtml_boundary_marker_,
      CreateFrameRoutingIdToContentId(rfh->GetSiteInstance()), is_last_frame));
  return true;
}

void MHTMLGenerationManager::Job::RenderProcessExited(
    RenderProcessHost* host,
    base::TerminationStatus status,
    int exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MHTMLGenerationManager::GetInstance()->RenderProcessExited(this);
}

void MHTMLGenerationManager::Job::AddFrame(RenderFrameHost* render_frame_host) {
  auto* rfhi = static_cast<RenderFrameHostImpl*>(render_frame_host);
  int frame_tree_node_id = rfhi->frame_tree_node()->frame_tree_node_id();
  pending_frame_tree_node_ids_.push(frame_tree_node_id);

  std::string guid = base::GenerateGUID();
  std::string content_id = base::StringPrintf("<frame-%d-%s@mhtml.blink>",
                                              frame_tree_node_id, guid.c_str());
  frame_tree_node_to_content_id_[frame_tree_node_id] = content_id;
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

// static
std::string MHTMLGenerationManager::Job::GenerateMHTMLBoundaryMarker() {
  // TODO(lukasza): Introduce and use a shared helper function in
  // net/base/mime_util.h instead of having the ad-hoc code below.

  // Trying to generate random boundaries similar to IE/UnMHT
  // (ex: ----=_NextPart_000_001B_01CC157B.96F808A0).
  uint8_t random_values[10];
  base::RandBytes(&random_values, sizeof(random_values));

  std::string result("----=_NextPart_000_");
  result += base::HexEncode(random_values + 0, 2);
  result += '_';
  result += base::HexEncode(random_values + 2, 4);
  result += '.';
  result += base::HexEncode(random_values + 6, 4);
  return result;
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

void MHTMLGenerationManager::OnSavedFrameAsMHTML(
    int job_id,
    bool mhtml_generation_in_renderer_succeeded) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!mhtml_generation_in_renderer_succeeded) {
    JobFinished(job_id, JobStatus::FAILURE);
    return;
  }

  Job* job = FindJob(job_id);
  if (!job)
    return;

  if (job->HasMoreFramesToProcess()) {
    if (!job->SendToNextRenderFrame()) {
      JobFinished(job_id, JobStatus::FAILURE);
    }
    return;
  }

  JobFinished(job_id, JobStatus::SUCCESS);
}

// static
base::File MHTMLGenerationManager::CreateFile(const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // SECURITY NOTE: A file descriptor to the file created below will be passed
  // to multiple renderer processes which (in out-of-process iframes mode) can
  // act on behalf of separate web principals.  Therefore it is important to
  // only allow writing to the file and forbid reading from the file (as this
  // would allow reading content generated by other renderers / other web
  // principals).
  uint32 file_flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;

  base::File browser_file(file_path, file_flags);
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

  if (!job->SendToNextRenderFrame()) {
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
