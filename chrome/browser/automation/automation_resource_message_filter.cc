// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_resource_message_filter.h"

#include "base/histogram.h"
#include "base/path_service.h"
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
  URLRequestAutomationJob::InitializeInterceptor();
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
  int request_id = URLRequestAutomationJob::MayFilterMessage(message);
  if (request_id) {
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
  DCHECK(request_map_.end() == request_map_.find(job->id()));
  request_map_[job->id()] = job;
  return true;
}

void AutomationResourceMessageFilter::UnRegisterRequest(
    URLRequestAutomationJob* job) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(request_map_.find(job->id()) != request_map_.end());
  request_map_.erase(job->id());
}

bool AutomationResourceMessageFilter::RegisterRenderView(
    int renderer_pid, int renderer_id, int tab_handle,
    AutomationResourceMessageFilter* filter) {
  if (!renderer_pid || !renderer_id || !tab_handle) {
    NOTREACHED();
    return false;
  }

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(
          AutomationResourceMessageFilter::RegisterRenderViewInIOThread,
          renderer_pid, renderer_id, tab_handle, filter));
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

void AutomationResourceMessageFilter::RegisterRenderViewInIOThread(
    int renderer_pid, int renderer_id,
    int tab_handle, AutomationResourceMessageFilter* filter) {
  RenderViewMap::iterator automation_details_iter(
      filtered_render_views_.find(RendererId(renderer_pid, renderer_id)));
  if (automation_details_iter != filtered_render_views_.end()) {
    DCHECK(automation_details_iter->second.ref_count > 0);
    automation_details_iter->second.ref_count++;
  } else {
    filtered_render_views_[RendererId(renderer_pid, renderer_id)] =
        AutomationDetails(tab_handle, filter);
  }
}

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

