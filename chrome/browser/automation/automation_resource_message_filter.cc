// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_resource_message_filter.h"

#include "base/path_service.h"
#include "base/metrics/histogram.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/automation/url_request_automation_job.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/net/url_request_failed_dns_job.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/net/url_request_slow_download_job.h"
#include "chrome/browser/net/url_request_slow_http_job.h"
#include "chrome/browser/renderer_host/render_message_filter.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_paths.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_filter.h"

base::LazyInstance<AutomationResourceMessageFilter::RenderViewMap>
    AutomationResourceMessageFilter::filtered_render_views_(
        base::LINKER_INITIALIZED);

base::LazyInstance<AutomationResourceMessageFilter::CompletionCallbackMap>
    AutomationResourceMessageFilter::completion_callback_map_(
        base::LINKER_INITIALIZED);

int AutomationResourceMessageFilter::unique_request_id_ = 1;
int AutomationResourceMessageFilter::next_completion_callback_id_ = 0;

// CookieStore specialization to enable fetching and setting cookies over the
// automation channel. This cookie store is transient i.e. it maintains cookies
// for the duration of the current request to set or get cookies from the
// renderer.
class AutomationCookieStore : public net::CookieStore {
 public:
  AutomationCookieStore(AutomationResourceMessageFilter* automation_client,
                        int tab_handle)
      : automation_client_(automation_client),
        tab_handle_(tab_handle) {
  }

  virtual ~AutomationCookieStore() {
    DVLOG(1) << "In " << __FUNCTION__;
  }

  // CookieStore implementation.
  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const net::CookieOptions& options) {
    // The cookie_string_ is available only once, i.e. once it is read by
    // it is invalidated.
    cookie_string_ = cookie_line;
    return true;
  }

  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const net::CookieOptions& options) {
    return cookie_string_;
  }

  virtual void DeleteCookie(const GURL& url,
                            const std::string& cookie_name) {
    NOTREACHED() << "Should not get called for an automation profile";
  }

  virtual net::CookieMonster* GetCookieMonster() {
    NOTREACHED() << "Should not get called for an automation profile";
    return NULL;
  }

 protected:
  scoped_refptr<AutomationResourceMessageFilter> automation_client_;
  int tab_handle_;
  std::string cookie_string_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomationCookieStore);
};

AutomationResourceMessageFilter::AutomationDetails::AutomationDetails()
    : tab_handle(0),
      ref_count(1),
      is_pending_render_view(false) {
}

AutomationResourceMessageFilter::AutomationDetails::AutomationDetails(
    int tab,
    AutomationResourceMessageFilter* flt,
    bool pending_view)
    : tab_handle(tab), ref_count(1), filter(flt),
      is_pending_render_view(pending_view) {
}

AutomationResourceMessageFilter::AutomationDetails::~AutomationDetails() {}

struct AutomationResourceMessageFilter::CookieCompletionInfo {
  net::CompletionCallback* completion_callback;
  scoped_refptr<net::CookieStore> cookie_store;
};

AutomationResourceMessageFilter::AutomationResourceMessageFilter()
    : channel_(NULL) {
  // Ensure that an instance of the callback map is created.
  completion_callback_map_.Get();
  // Ensure that an instance of the render view map is created.
  filtered_render_views_.Get();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(
          URLRequestAutomationJob::EnsureProtocolFactoryRegistered));
}

AutomationResourceMessageFilter::~AutomationResourceMessageFilter() {
}

// Called on the IPC thread:
void AutomationResourceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(!channel_);
  channel_ = channel;
}

void AutomationResourceMessageFilter::OnFilterRemoved() {
  channel_ = NULL;
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
  RenderViewMap::iterator index = filtered_render_views_.Get().begin();
  while (index != filtered_render_views_.Get().end()) {
    const AutomationDetails& details = (*index).second;
    if (details.filter.get() == this) {
      filtered_render_views_.Get().erase(index++);
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
    } else {
      // This could occur if the request was stopped from Chrome which would
      // delete it from the request map. If we receive data for this request
      // from the host we should ignore it.
      LOG(ERROR) << "Failed to find request id:" << request_id;
      return true;
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
    IPC_MESSAGE_HANDLER(AutomationMsg_GetCookiesHostResponse,
                        OnGetCookiesHostResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

// Called on the IPC thread:
bool AutomationResourceMessageFilter::Send(IPC::Message* message) {
  // This has to be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (job->is_pending()) {
    DCHECK(ContainsKey(pending_request_map_, job->id()));
    pending_request_map_.erase(job->id());
  } else {
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

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(
          AutomationResourceMessageFilter::RegisterRenderViewInIOThread,
          renderer_pid,
          renderer_id,
          tab_handle,
          make_scoped_refptr(filter),
          pending_view));
  return true;
}

void AutomationResourceMessageFilter::UnRegisterRenderView(
    int renderer_pid, int renderer_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
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

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(
          AutomationResourceMessageFilter::ResumePendingRenderViewInIOThread,
          renderer_pid,
          renderer_id,
          tab_handle,
          make_scoped_refptr(filter)));
  return true;
}

void AutomationResourceMessageFilter::RegisterRenderViewInIOThread(
    int renderer_pid, int renderer_id,
    int tab_handle, AutomationResourceMessageFilter* filter,
    bool pending_view) {
  RendererId renderer_key(renderer_pid, renderer_id);

  scoped_refptr<net::CookieStore> cookie_store(
      new AutomationCookieStore(filter, tab_handle));

  RenderViewMap::iterator automation_details_iter(
      filtered_render_views_.Get().find(renderer_key));
  if (automation_details_iter != filtered_render_views_.Get().end()) {
    DCHECK(automation_details_iter->second.ref_count > 0);
    automation_details_iter->second.ref_count++;
  } else {
    filtered_render_views_.Get()[renderer_key] =
        AutomationDetails(tab_handle, filter, pending_view);
  }

  filtered_render_views_.Get()[renderer_key].set_cookie_store(cookie_store);
}

// static
void AutomationResourceMessageFilter::UnRegisterRenderViewInIOThread(
    int renderer_pid, int renderer_id) {
  RenderViewMap::iterator automation_details_iter(
      filtered_render_views_.Get().find(RendererId(renderer_pid,
                                                   renderer_id)));

  if (automation_details_iter == filtered_render_views_.Get().end()) {
    VLOG(1) << "UnRegisterRenderViewInIOThread: already unregistered";
    return;
  }

  automation_details_iter->second.ref_count--;

  if (automation_details_iter->second.ref_count <= 0) {
    filtered_render_views_.Get().erase(RendererId(renderer_pid, renderer_id));
  }
}

// static
bool AutomationResourceMessageFilter::ResumePendingRenderViewInIOThread(
    int renderer_pid, int renderer_id, int tab_handle,
    AutomationResourceMessageFilter* filter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  RendererId renderer_key(renderer_pid, renderer_id);

  RenderViewMap::iterator automation_details_iter(
      filtered_render_views_.Get().find(renderer_key));

  if (automation_details_iter == filtered_render_views_.Get().end()) {
    NOTREACHED() << "Failed to find pending view for renderer pid:"
                 << renderer_pid
                 << ", render view id:"
                 << renderer_id;
    return false;
  }

  DCHECK(automation_details_iter->second.is_pending_render_view);

  scoped_refptr<net::CookieStore> cookie_store(
      new AutomationCookieStore(filter, tab_handle));

  AutomationResourceMessageFilter* old_filter =
      automation_details_iter->second.filter;
  DCHECK(old_filter != NULL);

  filtered_render_views_.Get()[renderer_key] =
      AutomationDetails(tab_handle, filter, false);

  filtered_render_views_.Get()[renderer_key].set_cookie_store(cookie_store);

  ResumeJobsForPendingView(tab_handle, old_filter, filter);
  return true;
}

bool AutomationResourceMessageFilter::LookupRegisteredRenderView(
    int renderer_pid, int renderer_id, AutomationDetails* details) {
  bool found = false;
  RenderViewMap::iterator it = filtered_render_views_.Get().find(RendererId(
      renderer_pid, renderer_id));
  if (it != filtered_render_views_.Get().end()) {
    found = true;
    if (details)
      *details = it->second;
  }

  return found;
}

bool AutomationResourceMessageFilter::GetAutomationRequestId(
    int request_id, int* automation_request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

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
    LOG(ERROR) << "Invalid request id: " << request_id;
    return false;
  }

  return Send(new AutomationMsg_DownloadRequestInHost(tab_handle,
                                                      automation_request_id));
}

void AutomationResourceMessageFilter::OnSetFilteredInet(bool enable) {
  chrome_browser_net::SetUrlRequestMocksEnabled(enable);
}

void AutomationResourceMessageFilter::OnGetFilteredInetHitCount(
    int* hit_count) {
  *hit_count = net::URLRequestFilter::GetInstance()->hit_count();
}

void AutomationResourceMessageFilter::OnRecordHistograms(
    const std::vector<std::string>& histogram_list) {
  for (size_t index = 0; index < histogram_list.size(); ++index) {
    base::Histogram::DeserializeHistogramInfo(histogram_list[index]);
  }
}

bool AutomationResourceMessageFilter::GetCookiesForUrl(
    const GURL& url, net::CompletionCallback* callback) {
  GetCookiesCompletion* get_cookies_callback =
      static_cast<GetCookiesCompletion*>(callback);

  RendererId renderer_key(get_cookies_callback->render_process_id(),
      get_cookies_callback->render_view_id());

  RenderViewMap::iterator automation_details_iter(
        filtered_render_views_.Get().find(renderer_key));

  if (automation_details_iter == filtered_render_views_.Get().end()) {
    return false;
  }

  DCHECK(automation_details_iter->second.filter != NULL);
  DCHECK(automation_details_iter->second.cookie_store_.get() != NULL);

  int completion_callback_id = GetNextCompletionCallbackId();
  DCHECK(!ContainsKey(completion_callback_map_.Get(), completion_callback_id));

  CookieCompletionInfo cookie_info;
  cookie_info.completion_callback = callback;
  cookie_info.cookie_store = automation_details_iter->second.cookie_store_;

  completion_callback_map_.Get()[completion_callback_id] = cookie_info;

  DCHECK(automation_details_iter->second.filter != NULL);

  if (automation_details_iter->second.filter) {
    automation_details_iter->second.filter->Send(
        new AutomationMsg_GetCookiesFromHost(
            automation_details_iter->second.tab_handle, url,
            completion_callback_id));
  }
  return true;
}

void AutomationResourceMessageFilter::OnGetCookiesHostResponse(
    int tab_handle, bool success, const GURL& url, const std::string& cookies,
    int cookie_id) {
  CompletionCallbackMap::iterator index =
      completion_callback_map_.Get().find(cookie_id);
  if (index != completion_callback_map_.Get().end()) {
    net::CompletionCallback* callback = index->second.completion_callback;

    scoped_refptr<net::CookieStore> cookie_store = index->second.cookie_store;

    DCHECK(callback != NULL);
    DCHECK(cookie_store.get() != NULL);

    completion_callback_map_.Get().erase(index);

    OnGetCookiesHostResponseInternal(tab_handle, success, url, cookies,
                                     callback, cookie_store.get());
  } else {
    NOTREACHED() << "Received invalid completion callback id:"
                 << cookie_id;
  }
}

void AutomationResourceMessageFilter::OnGetCookiesHostResponseInternal(
    int tab_handle, bool success, const GURL& url, const std::string& cookies,
  net::CompletionCallback* callback, net::CookieStore* cookie_store) {
  DCHECK(callback);
  DCHECK(cookie_store);

  GetCookiesCompletion* get_cookies_callback =
      static_cast<GetCookiesCompletion*>(callback);

  get_cookies_callback->set_cookie_store(cookie_store);

  // Set the cookie in the cookie store so that the callback can read it.
  cookie_store->SetCookieWithOptions(url, cookies, net::CookieOptions());

  Tuple1<int> params;
  params.a = success ? net::OK : net::ERR_ACCESS_DENIED;
  callback->RunWithParams(params);

  // The cookie for this URL is only valid until it is read by the callback.
  cookie_store->SetCookieWithOptions(url, "", net::CookieOptions());
}

bool AutomationResourceMessageFilter::SetCookiesForUrl(
    const GURL& url, const std::string& cookie_line,
    net::CompletionCallback* callback) {
  SetCookieCompletion* set_cookies_callback =
      static_cast<SetCookieCompletion*>(callback);

  RenderViewMap::iterator automation_details_iter(
        filtered_render_views_.Get().find(RendererId(
            set_cookies_callback->render_process_id(),
            set_cookies_callback->render_view_id())));

  if (automation_details_iter == filtered_render_views_.Get().end()) {
    return false;
  }

  DCHECK(automation_details_iter->second.filter != NULL);

  if (automation_details_iter->second.filter) {
    automation_details_iter->second.filter->Send(
        new AutomationMsg_SetCookieAsync(
            automation_details_iter->second.tab_handle, url, cookie_line));
  }

  return true;
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
