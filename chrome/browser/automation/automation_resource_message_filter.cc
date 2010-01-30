// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_resource_message_filter.h"

#include "base/histogram.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/automation/url_request_automation_job.h"
#include "chrome/browser/net/url_request_failed_dns_job.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/net/url_request_slow_download_job.h"
#include "chrome/browser/net/url_request_slow_http_job.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/test/automation/automation_messages.h"
#include "net/url_request/url_request_filter.h"


AutomationResourceMessageFilter::RenderViewMap
    AutomationResourceMessageFilter::filtered_render_views_;

int AutomationResourceMessageFilter::unique_request_id_ = 1;

AutomationResourceMessageFilter::AutomationResourceMessageFilter()
    : channel_(NULL) {
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(
          URLRequestAutomationJob::EnsureProtocolFactoryRegistered));
}

AutomationResourceMessageFilter::~AutomationResourceMessageFilter() {
}

// Called on the IPC thread:
void AutomationResourceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(channel_ == NULL);
  channel_ = channel;
}

// Called on the IPC thread:
void AutomationResourceMessageFilter::OnChannelConnected(int32 peer_pid) {
}

// Called on the IPC thread:
void AutomationResourceMessageFilter::OnChannelClosing() {
  channel_ = NULL;
  request_map_.clear();

  // Only erase RenderViews which are associated with this
  // AutomationResourceMessageFilter instance.
  RenderViewMap::iterator index = filtered_render_views_.begin();
  while (index != filtered_render_views_.end()) {
    const AutomationDetails& details = (*index).second;
    if (details.filter.get() == this) {
      filtered_render_views_.erase(index++);
    } else {
      index++;
    }
  }
}

// Called on the IPC thread:
bool AutomationResourceMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  int request_id;
  if (URLRequestAutomationJob::MayFilterMessage(message, &request_id)) {
    RequestMap::iterator it = request_map_.find(request_id);
    if (it != request_map_.end()) {
      URLRequestAutomationJob* job = it->second;
      DCHECK(job);
      if (job) {
        job->OnMessage(message);
        return true;
      }
    }
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutomationResourceMessageFilter, message)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetFilteredInet,
                        OnSetFilteredInet)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetFilteredInetHitCount,
                        OnGetFilteredInetHitCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_RecordHistograms,
                        OnRecordHistograms)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

// Called on the IPC thread:
bool AutomationResourceMessageFilter::Send(IPC::Message* message) {
  // This has to be called on the IO thread.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

bool AutomationResourceMessageFilter::RegisterRequest(
    URLRequestAutomationJob* job) {
  if (!job) {
    NOTREACHED();
    return false;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // Register pending jobs in the pending request map for servicing later.
  if (job->is_pending()) {
    DCHECK(!ContainsKey(pending_request_map_, job->id()));
    DCHECK(!ContainsKey(request_map_, job->id()));
    pending_request_map_[job->id()] = job;
  } else {
    DCHECK(!ContainsKey(request_map_, job->id()));
    DCHECK(!ContainsKey(pending_request_map_, job->id()));
    request_map_[job->id()] = job;
  }

  return true;
}

void AutomationResourceMessageFilter::UnRegisterRequest(
    URLRequestAutomationJob* job) {
  if (!job) {
    NOTREACHED();
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (job->is_pending()) {
    DCHECK(ContainsKey(pending_request_map_, job->id()));
    pending_request_map_.erase(job->id());
  } else {
    DCHECK(ContainsKey(request_map_, job->id()));
    request_map_.erase(job->id());
  }
}

bool AutomationResourceMessageFilter::RegisterRenderView(
    int renderer_pid, int renderer_id, int tab_handle,
    AutomationResourceMessageFilter* filter,
    bool pending_view) {
  if (!renderer_pid || !renderer_id || !tab_handle) {
    NOTREACHED();
    return false;
  }

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(
          AutomationResourceMessageFilter::RegisterRenderViewInIOThread,
          renderer_pid, renderer_id, tab_handle, filter, pending_view));
  return true;
}

void AutomationResourceMessageFilter::UnRegisterRenderView(
    int renderer_pid, int renderer_id) {
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(
          AutomationResourceMessageFilter::UnRegisterRenderViewInIOThread,
          renderer_pid, renderer_id));
}

bool AutomationResourceMessageFilter::ResumePendingRenderView(
    int renderer_pid, int renderer_id, int tab_handle,
    AutomationResourceMessageFilter* filter) {
  if (!renderer_pid || !renderer_id || !tab_handle) {
    NOTREACHED();
    return false;
  }

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(
          AutomationResourceMessageFilter::ResumePendingRenderViewInIOThread,
          renderer_pid, renderer_id, tab_handle, filter));
  return true;
}

void AutomationResourceMessageFilter::RegisterRenderViewInIOThread(
    int renderer_pid, int renderer_id,
    int tab_handle, AutomationResourceMessageFilter* filter,
    bool pending_view) {
  RenderViewMap::iterator automation_details_iter(
      filtered_render_views_.find(RendererId(renderer_pid, renderer_id)));
  if (automation_details_iter != filtered_render_views_.end()) {
    DCHECK(automation_details_iter->second.ref_count > 0);
    automation_details_iter->second.ref_count++;
  } else {
    filtered_render_views_[RendererId(renderer_pid, renderer_id)] =
        AutomationDetails(tab_handle, filter, pending_view);
  }
}

// static
void AutomationResourceMessageFilter::UnRegisterRenderViewInIOThread(
    int renderer_pid, int renderer_id) {
  RenderViewMap::iterator automation_details_iter(
      filtered_render_views_.find(RendererId(renderer_pid, renderer_id)));

  if (automation_details_iter == filtered_render_views_.end()) {
    LOG(INFO) << "UnRegisterRenderViewInIOThread: already unregistered";
    return;
  }

  automation_details_iter->second.ref_count--;

  if (automation_details_iter->second.ref_count <= 0) {
    filtered_render_views_.erase(RendererId(renderer_pid, renderer_id));
  }
}

// static
bool AutomationResourceMessageFilter::ResumePendingRenderViewInIOThread(
    int renderer_pid, int renderer_id, int tab_handle,
    AutomationResourceMessageFilter* filter) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  RenderViewMap::iterator automation_details_iter(
      filtered_render_views_.find(RendererId(renderer_pid, renderer_id)));

  if (automation_details_iter == filtered_render_views_.end()) {
    NOTREACHED() << "Failed to find pending view for renderer pid:"
                 << renderer_pid
                 << ", render view id:"
                 << renderer_id;
    return false;
  }

  DCHECK(automation_details_iter->second.is_pending_render_view);

  AutomationResourceMessageFilter* old_filter =
      automation_details_iter->second.filter;
  DCHECK(old_filter != NULL);

  filtered_render_views_[RendererId(renderer_pid, renderer_id)] =
      AutomationDetails(tab_handle, filter, false);

  ResumeJobsForPendingView(tab_handle, old_filter, filter);
  return true;
}

bool AutomationResourceMessageFilter::LookupRegisteredRenderView(
    int renderer_pid, int renderer_id, AutomationDetails* details) {
  bool found = false;
  RenderViewMap::iterator it = filtered_render_views_.find(RendererId(
      renderer_pid, renderer_id));
  if (it != filtered_render_views_.end()) {
    found = true;
    if (details)
      *details = it->second;
  }

  return found;
}

bool AutomationResourceMessageFilter::GetAutomationRequestId(
    int request_id, int* automation_request_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  RequestMap::iterator it = request_map_.begin();
  while (it != request_map_.end()) {
    URLRequestAutomationJob* job = it->second;
    DCHECK(job);
    if (job && job->request_id() == request_id) {
      *automation_request_id = job->id();
      return true;
    }
    it++;
  }

  return false;
}

bool AutomationResourceMessageFilter::SendDownloadRequestToHost(
    int routing_id, int tab_handle, int request_id) {
  int automation_request_id = 0;
  bool valid_id = GetAutomationRequestId(request_id, &automation_request_id);
  if (!valid_id) {
    NOTREACHED() << "Invalid request id: " << request_id;
    return false;
  }

  return Send(new AutomationMsg_DownloadRequestInHost(0, tab_handle,
                                                      automation_request_id));
}

void AutomationResourceMessageFilter::OnSetFilteredInet(bool enable) {
  chrome_browser_net::SetUrlRequestMocksEnabled(enable);
}

void AutomationResourceMessageFilter::OnGetFilteredInetHitCount(
    int* hit_count) {
  *hit_count = URLRequestFilter::GetInstance()->hit_count();
}

void AutomationResourceMessageFilter::OnRecordHistograms(
    const std::vector<std::string>& histogram_list) {
  for (size_t index = 0; index < histogram_list.size(); ++index) {
    Histogram::DeserializeHistogramInfo(histogram_list[index]);
  }
}

// static
void AutomationResourceMessageFilter::ResumeJobsForPendingView(
    int tab_handle,
    AutomationResourceMessageFilter* old_filter,
    AutomationResourceMessageFilter* new_filter) {
  DCHECK(old_filter != NULL);
  DCHECK(new_filter != NULL);

  RequestMap pending_requests = old_filter->pending_request_map_;

  for (RequestMap::iterator index = old_filter->pending_request_map_.begin();
          index != old_filter->pending_request_map_.end(); index++) {
    scoped_refptr<URLRequestAutomationJob> job = (*index).second;
    DCHECK_EQ(job->message_filter(), old_filter);
    DCHECK(job->is_pending());
    // StartPendingJob will register the job with the new filter.
    job->StartPendingJob(tab_handle, new_filter);
  }

  old_filter->pending_request_map_.clear();
}
