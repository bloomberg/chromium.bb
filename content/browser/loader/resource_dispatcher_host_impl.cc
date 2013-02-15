// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/browser/loader/resource_dispatcher_host_impl.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/shared_memory.h"
#include "base/stl_util.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/cert_store_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/cross_site_request_manager.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/download/save_file_resource_handler.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/loader/async_resource_handler.h"
#include "content/browser/loader/buffered_resource_handler.h"
#include "content/browser/loader/cross_site_resource_handler.h"
#include "content/browser/loader/power_save_block_resource_throttle.h"
#include "content/browser/loader/redirect_to_file_resource_handler.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/sync_resource_handler.h"
#include "content/browser/loader/throttling_resource_handler.h"
#include "content/browser/loader/transfer_navigation_resource_throttle.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "content/common/resource_messages.h"
#include "content/common/ssl_status_serialization.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "net/base/auth.h"
#include "net/base/cert_status_flags.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/request_priority.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/upload_data_stream.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "webkit/appcache/appcache_interceptor.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/glue/resource_request_body.h"
#include "webkit/glue/webkit_glue.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using webkit_blob::ShareableFileReference;
using webkit_glue::ResourceRequestBody;

// ----------------------------------------------------------------------------

namespace content {

namespace {

static ResourceDispatcherHostImpl* g_resource_dispatcher_host;

// The interval for calls to ResourceDispatcherHostImpl::UpdateLoadStates
const int kUpdateLoadStatesIntervalMsec = 100;

// Maximum byte "cost" of all the outstanding requests for a renderer.
// See delcaration of |max_outstanding_requests_cost_per_process_| for details.
// This bound is 25MB, which allows for around 6000 outstanding requests.
const int kMaxOutstandingRequestsCostPerProcess = 26214400;

// The number of milliseconds after noting a user gesture that we will
// tag newly-created URLRequest objects with the
// net::LOAD_MAYBE_USER_GESTURE load flag. This is a fairly arbitrary
// guess at how long to expect direct impact from a user gesture, but
// this should be OK as the load flag is a best-effort thing only,
// rather than being intended as fully accurate.
const int kUserGestureWindowMs = 3500;

// All possible error codes from the network module. Note that the error codes
// are all positive (since histograms expect positive sample values).
const int kAllNetErrorCodes[] = {
#define NET_ERROR(label, value) -(value),
#include "net/base/net_error_list.h"
#undef NET_ERROR
};

// Aborts a request before an URLRequest has actually been created.
void AbortRequestBeforeItStarts(ResourceMessageFilter* filter,
                                IPC::Message* sync_result,
                                int route_id,
                                int request_id) {
  if (sync_result) {
    SyncLoadResult result;
    result.error_code = net::ERR_ABORTED;
    ResourceHostMsg_SyncLoad::WriteReplyParams(sync_result, result);
    filter->Send(sync_result);
  } else {
    // Tell the renderer that this request was disallowed.
    filter->Send(new ResourceMsg_RequestComplete(
        route_id,
        request_id,
        net::ERR_ABORTED,
        false,
        std::string(),   // No security info needed, connection not established.
        base::TimeTicks()));
  }
}

GURL MaybeStripReferrer(const GURL& possible_referrer) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoReferrers))
    return GURL();
  return possible_referrer;
}

// Consults the RendererSecurity policy to determine whether the
// ResourceDispatcherHostImpl should service this request.  A request might be
// disallowed if the renderer is not authorized to retrieve the request URL or
// if the renderer is attempting to upload an unauthorized file.
bool ShouldServiceRequest(ProcessType process_type,
                          int child_id,
                          const ResourceHostMsg_Request& request_data)  {
  if (process_type == PROCESS_TYPE_PLUGIN)
    return true;

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Check if the renderer is permitted to request the requested URL.
  if (!policy->CanRequestURL(child_id, request_data.url)) {
    VLOG(1) << "Denied unauthorized request for "
            << request_data.url.possibly_invalid_spec();
    return false;
  }

  // Check if the renderer is permitted to upload the requested files.
  if (request_data.request_body) {
    const std::vector<ResourceRequestBody::Element>* uploads =
        request_data.request_body->elements();
    std::vector<ResourceRequestBody::Element>::const_iterator iter;
    for (iter = uploads->begin(); iter != uploads->end(); ++iter) {
      if (iter->type() == ResourceRequestBody::Element::TYPE_FILE &&
          !policy->CanReadFile(child_id, iter->path())) {
        NOTREACHED() << "Denied unauthorized upload of "
                     << iter->path().value();
        return false;
      }
    }
  }

  return true;
}

void RemoveDownloadFileFromChildSecurityPolicy(int child_id,
                                               const base::FilePath& path) {
  ChildProcessSecurityPolicyImpl::GetInstance()->RevokeAllPermissionsForFile(
      child_id, path);
}

#if defined(OS_WIN)
#pragma warning(disable: 4748)
#pragma optimize("", off)
#endif

#if defined(OS_WIN)
#pragma optimize("", on)
#pragma warning(default: 4748)
#endif

net::RequestPriority DetermineRequestPriority(
    const ResourceHostMsg_Request& request_data) {
  switch (request_data.priority) {
    case WebKit::WebURLRequest::PriorityVeryHigh:
      return net::HIGHEST;

    case WebKit::WebURLRequest::PriorityHigh:
      return net::MEDIUM;

    case WebKit::WebURLRequest::PriorityMedium:
      return net::LOW;

    case WebKit::WebURLRequest::PriorityLow:
      return net::LOWEST;

    case WebKit::WebURLRequest::PriorityVeryLow:
      return net::IDLE;

    case WebKit::WebURLRequest::PriorityUnresolved:
    default:
      NOTREACHED();
      return net::LOW;
  }
}

void OnSwapOutACKHelper(int render_process_id,
                        int render_view_id,
                        bool timed_out) {
  RenderViewHostImpl* rvh = RenderViewHostImpl::FromID(render_process_id,
                                                       render_view_id);
  if (rvh)
    rvh->OnSwapOutACK(timed_out);
}

net::Error CallbackAndReturn(
    const DownloadResourceHandler::OnStartedCallback& started_cb,
    net::Error net_error) {
  if (started_cb.is_null())
    return net_error;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(started_cb, static_cast<DownloadItem*>(NULL), net_error));

  return net_error;
}

int BuildLoadFlagsForRequest(const ResourceHostMsg_Request& request_data,
                             int child_id, bool is_sync_load) {
  int load_flags = request_data.load_flags;

  // Although EV status is irrelevant to sub-frames and sub-resources, we have
  // to perform EV certificate verification on all resources because an HTTP
  // keep-alive connection created to load a sub-frame or a sub-resource could
  // be reused to load a main frame.
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (request_data.resource_type == ResourceType::MAIN_FRAME) {
    load_flags |= net::LOAD_MAIN_FRAME;
  } else if (request_data.resource_type == ResourceType::SUB_FRAME) {
    load_flags |= net::LOAD_SUB_FRAME;
  } else if (request_data.resource_type == ResourceType::PREFETCH) {
    load_flags |= (net::LOAD_PREFETCH | net::LOAD_DO_NOT_PROMPT_FOR_LOGIN);
  } else if (request_data.resource_type == ResourceType::FAVICON) {
    load_flags |= net::LOAD_DO_NOT_PROMPT_FOR_LOGIN;
  }

  if (is_sync_load)
    load_flags |= net::LOAD_IGNORE_LIMITS;

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanSendCookiesForOrigin(child_id, request_data.url)) {
    load_flags |= (net::LOAD_DO_NOT_SEND_COOKIES |
                   net::LOAD_DO_NOT_SEND_AUTH_DATA |
                   net::LOAD_DO_NOT_SAVE_COOKIES);
  }

  // Raw headers are sensitive, as they include Cookie/Set-Cookie, so only
  // allow requesting them if requester has ReadRawCookies permission.
  if ((load_flags & net::LOAD_REPORT_RAW_HEADERS)
      && !policy->CanReadRawCookies(child_id)) {
    VLOG(1) << "Denied unauthorized request for raw headers";
    load_flags &= ~net::LOAD_REPORT_RAW_HEADERS;
  }

  return load_flags;
}

int GetCertID(net::URLRequest* request, int child_id) {
  if (request->ssl_info().cert) {
    return CertStore::GetInstance()->StoreCert(request->ssl_info().cert,
                                               child_id);
  }
  return 0;
}

template <class T>
void NotifyOnUI(int type, int render_process_id, int render_view_id,
                scoped_ptr<T> detail) {
  RenderViewHostImpl* host =
      RenderViewHostImpl::FromID(render_process_id, render_view_id);
  if (host) {
    RenderViewHostDelegate* delegate = host->GetDelegate();
    NotificationService::current()->Notify(
        type, Source<WebContents>(delegate->GetAsWebContents()),
        Details<T>(detail.get()));
  }
}

}  // namespace

// static
ResourceDispatcherHost* ResourceDispatcherHost::Get() {
  return g_resource_dispatcher_host;
}

ResourceDispatcherHostImpl::ResourceDispatcherHostImpl()
    : save_file_manager_(new SaveFileManager()),
      request_id_(-1),
      is_shutdown_(false),
      max_outstanding_requests_cost_per_process_(
          kMaxOutstandingRequestsCostPerProcess),
      filter_(NULL),
      delegate_(NULL),
      allow_cross_origin_auth_prompt_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!g_resource_dispatcher_host);
  g_resource_dispatcher_host = this;

  GetContentClient()->browser()->ResourceDispatcherHostCreated();

  ANNOTATE_BENIGN_RACE(
      &last_user_gesture_time_,
      "We don't care about the precise value, see http://crbug.com/92889");

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&appcache::AppCacheInterceptor::EnsureRegistered));

  update_load_states_timer_.reset(
      new base::RepeatingTimer<ResourceDispatcherHostImpl>());
}

ResourceDispatcherHostImpl::~ResourceDispatcherHostImpl() {
  DCHECK(g_resource_dispatcher_host);
  g_resource_dispatcher_host = NULL;
}

// static
ResourceDispatcherHostImpl* ResourceDispatcherHostImpl::Get() {
  return g_resource_dispatcher_host;
}

void ResourceDispatcherHostImpl::SetDelegate(
    ResourceDispatcherHostDelegate* delegate) {
  delegate_ = delegate;
}

void ResourceDispatcherHostImpl::SetAllowCrossOriginAuthPrompt(bool value) {
  allow_cross_origin_auth_prompt_ = value;
}

void ResourceDispatcherHostImpl::AddResourceContext(ResourceContext* context) {
  active_resource_contexts_.insert(context);
}

void ResourceDispatcherHostImpl::RemoveResourceContext(
    ResourceContext* context) {
  CHECK(ContainsKey(active_resource_contexts_, context));
  active_resource_contexts_.erase(context);
}

void ResourceDispatcherHostImpl::CancelRequestsForContext(
    ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(context);

  CHECK(ContainsKey(active_resource_contexts_, context));

  // Note that request cancellation has side effects. Therefore, we gather all
  // the requests to cancel first, and then we start cancelling. We assert at
  // the end that there are no more to cancel since the context is about to go
  // away.
  typedef std::vector<linked_ptr<ResourceLoader> > LoaderList;
  LoaderList loaders_to_cancel;

  for (LoaderMap::iterator i = pending_loaders_.begin();
       i != pending_loaders_.end();) {
    if (i->second->GetRequestInfo()->GetContext() == context) {
      loaders_to_cancel.push_back(i->second);
      pending_loaders_.erase(i++);
    } else {
      ++i;
    }
  }

  for (BlockedLoadersMap::iterator i = blocked_loaders_map_.begin();
       i != blocked_loaders_map_.end();) {
    BlockedLoadersList* loaders = i->second;
    if (loaders->empty()) {
      // This can happen if BlockRequestsForRoute() has been called for a route,
      // but we haven't blocked any matching requests yet.
      ++i;
      continue;
    }
    ResourceRequestInfoImpl* info = loaders->front()->GetRequestInfo();
    if (info->GetContext() == context) {
      blocked_loaders_map_.erase(i++);
      for (BlockedLoadersList::const_iterator it = loaders->begin();
           it != loaders->end(); ++it) {
        linked_ptr<ResourceLoader> loader = *it;
        info = loader->GetRequestInfo();
        // We make the assumption that all requests on the list have the same
        // ResourceContext.
        DCHECK_EQ(context, info->GetContext());
        IncrementOutstandingRequestsMemoryCost(-1 * info->memory_cost(),
                                               info->GetChildID());
        loaders_to_cancel.push_back(loader);
      }
      delete loaders;
    } else {
      ++i;
    }
  }

#ifndef NDEBUG
  for (LoaderList::iterator i = loaders_to_cancel.begin();
       i != loaders_to_cancel.end(); ++i) {
    // There is no strict requirement that this be the case, but currently
    // downloads and transferred requests are the only requests that aren't
    // cancelled when the associated processes go away. It may be OK for this
    // invariant to change in the future, but if this assertion fires without
    // the invariant changing, then it's indicative of a leak.
    DCHECK((*i)->GetRequestInfo()->is_download() || (*i)->is_transferring());
  }
#endif

  loaders_to_cancel.clear();

  // Validate that no more requests for this context were added.
  for (LoaderMap::const_iterator i = pending_loaders_.begin();
       i != pending_loaders_.end(); ++i) {
    // http://crbug.com/90971
    CHECK_NE(i->second->GetRequestInfo()->GetContext(), context);
  }

  for (BlockedLoadersMap::const_iterator i = blocked_loaders_map_.begin();
       i != blocked_loaders_map_.end(); ++i) {
    BlockedLoadersList* loaders = i->second;
    if (!loaders->empty()) {
      ResourceRequestInfoImpl* info = loaders->front()->GetRequestInfo();
      // http://crbug.com/90971
      CHECK_NE(info->GetContext(), context);
    }
  }
}

net::Error ResourceDispatcherHostImpl::BeginDownload(
    scoped_ptr<net::URLRequest> request,
    bool is_content_initiated,
    ResourceContext* context,
    int child_id,
    int route_id,
    bool prefer_cache,
    scoped_ptr<DownloadSaveInfo> save_info,
    content::DownloadId download_id,
    const DownloadStartedCallback& started_callback) {
  if (is_shutdown_)
    return CallbackAndReturn(started_callback, net::ERR_INSUFFICIENT_RESOURCES);

  const GURL& url = request->original_url();

  // http://crbug.com/90971
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);
  CHECK(ContainsKey(active_resource_contexts_, context));

  request->set_referrer(MaybeStripReferrer(GURL(request->referrer())).spec());
  int extra_load_flags = net::LOAD_IS_DOWNLOAD;
  if (prefer_cache) {
    // If there is upload data attached, only retrieve from cache because there
    // is no current mechanism to prompt the user for their consent for a
    // re-post. For GETs, try to retrieve data from the cache and skip
    // validating the entry if present.
    if (request->get_upload() != NULL)
      extra_load_flags |= net::LOAD_ONLY_FROM_CACHE;
    else
      extra_load_flags |= net::LOAD_PREFERRING_CACHE;
  } else {
    extra_load_flags |= net::LOAD_DISABLE_CACHE;
  }
  request->set_load_flags(request->load_flags() | extra_load_flags);
  // Check if the renderer is permitted to request the requested URL.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->
          CanRequestURL(child_id, url)) {
    VLOG(1) << "Denied unauthorized download request for "
            << url.possibly_invalid_spec();
    return CallbackAndReturn(started_callback, net::ERR_ACCESS_DENIED);
  }

  request_id_--;

  const net::URLRequestContext* request_context = context->GetRequestContext();
  if (!request_context->job_factory()->IsHandledURL(url)) {
    VLOG(1) << "Download request for unsupported protocol: "
            << url.possibly_invalid_spec();
    return CallbackAndReturn(started_callback, net::ERR_ACCESS_DENIED);
  }

  ResourceRequestInfoImpl* extra_info =
      CreateRequestInfo(child_id, route_id, true, context);
  extra_info->AssociateWithRequest(request.get());  // Request takes ownership.

  // From this point forward, the |DownloadResourceHandler| is responsible for
  // |started_callback|.
  scoped_ptr<ResourceHandler> handler(
      CreateResourceHandlerForDownload(request.get(), is_content_initiated,
                                       true, download_id, save_info.Pass(),
                                       started_callback));

  BeginRequestInternal(request.Pass(), handler.Pass());

  return net::OK;
}

void ResourceDispatcherHostImpl::ClearLoginDelegateForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  if (info) {
    ResourceLoader* loader = GetLoader(info->GetGlobalRequestID());
    if (loader)
      loader->ClearLoginDelegate();
  }
}

void ResourceDispatcherHostImpl::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&ResourceDispatcherHostImpl::OnShutdown,
                                     base::Unretained(this)));
}

scoped_ptr<ResourceHandler>
ResourceDispatcherHostImpl::CreateResourceHandlerForDownload(
    net::URLRequest* request,
    bool is_content_initiated,
    bool must_download,
    DownloadId id,
    scoped_ptr<DownloadSaveInfo> save_info,
    const DownloadResourceHandler::OnStartedCallback& started_cb) {
  scoped_ptr<ResourceHandler> handler(
      new DownloadResourceHandler(id, request, started_cb, save_info.Pass()));
  if (delegate_) {
    const ResourceRequestInfo* request_info(
        ResourceRequestInfo::ForRequest(request));

    ScopedVector<ResourceThrottle> throttles;
    delegate_->DownloadStarting(
        request, request_info->GetContext(), request_info->GetChildID(),
        request_info->GetRouteID(), request_info->GetRequestID(),
        is_content_initiated, must_download, &throttles);
    if (!throttles.empty()) {
      handler.reset(
          new ThrottlingResourceHandler(
              handler.Pass(), request_info->GetChildID(),
              request_info->GetRequestID(), throttles.Pass()));
    }
  }
  return handler.Pass();
}

void ResourceDispatcherHostImpl::ClearSSLClientAuthHandlerForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  if (info) {
    ResourceLoader* loader = GetLoader(info->GetGlobalRequestID());
    if (loader)
      loader->ClearSSLClientAuthHandler();
  }
}

ResourceDispatcherHostLoginDelegate*
ResourceDispatcherHostImpl::CreateLoginDelegate(
    ResourceLoader* loader,
    net::AuthChallengeInfo* auth_info) {
  if (!delegate_)
    return NULL;

  return delegate_->CreateLoginDelegate(auth_info, loader->request());
}

bool ResourceDispatcherHostImpl::AcceptAuthRequest(
    ResourceLoader* loader,
    net::AuthChallengeInfo* auth_info) {
  if (delegate_ && !delegate_->AcceptAuthRequest(loader->request(), auth_info))
    return false;

  // Prevent third-party content from prompting for login, unless it is
  // a proxy that is trying to authenticate.  This is often the foundation
  // of a scam to extract credentials for another domain from the user.
  if (!auth_info->is_proxy) {
    HttpAuthResourceType resource_type =
        HttpAuthResourceTypeOf(loader->request());
    UMA_HISTOGRAM_ENUMERATION("Net.HttpAuthResource",
                              resource_type,
                              HTTP_AUTH_RESOURCE_LAST);

    // TODO(tsepez): Return false on HTTP_AUTH_RESOURCE_BLOCKED_CROSS.
    // The code once did this, but was changed due to http://crbug.com/174129.
    // http://crbug.com/174179 has been filed to track this issue.
  }

  return true;
}

bool ResourceDispatcherHostImpl::AcceptSSLClientCertificateRequest(
    ResourceLoader* loader,
    net::SSLCertRequestInfo* cert_info) {
  if (delegate_ && !delegate_->AcceptSSLClientCertificateRequest(
          loader->request(), cert_info)) {
    return false;
  }

  return true;
}

bool ResourceDispatcherHostImpl::HandleExternalProtocol(ResourceLoader* loader,
                                                        const GURL& url) {
  if (!delegate_)
    return false;

  ResourceRequestInfoImpl* info = loader->GetRequestInfo();

  if (!ResourceType::IsFrame(info->GetResourceType()))
    return false;

  const net::URLRequestJobFactory* job_factory =
      info->GetContext()->GetRequestContext()->job_factory();
  if (job_factory->IsHandledURL(url))
    return false;

  return delegate_->HandleExternalProtocol(url, info->GetChildID(),
                                           info->GetRouteID());
}

void ResourceDispatcherHostImpl::DidStartRequest(ResourceLoader* loader) {
  // Make sure we have the load state monitor running
  if (!update_load_states_timer_->IsRunning()) {
    update_load_states_timer_->Start(FROM_HERE,
        TimeDelta::FromMilliseconds(kUpdateLoadStatesIntervalMsec),
        this, &ResourceDispatcherHostImpl::UpdateLoadStates);
  }
}

void ResourceDispatcherHostImpl::DidReceiveRedirect(ResourceLoader* loader,
                                                    const GURL& new_url) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();

  int render_process_id, render_view_id;
  if (!info->GetAssociatedRenderView(&render_process_id, &render_view_id))
    return;

  // Notify the observers on the UI thread.
  scoped_ptr<ResourceRedirectDetails> detail(new ResourceRedirectDetails(
      loader->request(),
      GetCertID(loader->request(), info->GetChildID()),
      new_url));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &NotifyOnUI<ResourceRedirectDetails>,
          static_cast<int>(NOTIFICATION_RESOURCE_RECEIVED_REDIRECT),
          render_process_id, render_view_id, base::Passed(&detail)));
}

void ResourceDispatcherHostImpl::DidReceiveResponse(ResourceLoader* loader) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();

  int render_process_id, render_view_id;
  if (!info->GetAssociatedRenderView(&render_process_id, &render_view_id))
    return;

  // Notify the observers on the UI thread.
  scoped_ptr<ResourceRequestDetails> detail(new ResourceRequestDetails(
      loader->request(),
      GetCertID(loader->request(), info->GetChildID())));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &NotifyOnUI<ResourceRequestDetails>,
          static_cast<int>(NOTIFICATION_RESOURCE_RESPONSE_STARTED),
          render_process_id, render_view_id, base::Passed(&detail)));
}

void ResourceDispatcherHostImpl::DidFinishLoading(ResourceLoader* loader) {
  ResourceRequestInfo* info = loader->GetRequestInfo();

  // Record final result of all resource loads.
  if (info->GetResourceType() == ResourceType::MAIN_FRAME) {
    // This enumeration has "3" appended to its name to distinguish it from
    // older versions.
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(
        "Net.ErrorCodesForMainFrame3",
        -loader->request()->status().error(),
        base::CustomHistogram::ArrayToCustomRanges(
            kAllNetErrorCodes, arraysize(kAllNetErrorCodes)));

    if (loader->request()->url().SchemeIsSecure() &&
        loader->request()->url().host() == "www.google.com") {
      UMA_HISTOGRAM_CUSTOM_ENUMERATION(
          "Net.ErrorCodesForHTTPSGoogleMainFrame2",
          -loader->request()->status().error(),
          base::CustomHistogram::ArrayToCustomRanges(
              kAllNetErrorCodes, arraysize(kAllNetErrorCodes)));
    }
  } else {
    // This enumeration has "2" appended to distinguish it from older versions.
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(
        "Net.ErrorCodesForSubresources2",
        -loader->request()->status().error(),
        base::CustomHistogram::ArrayToCustomRanges(
            kAllNetErrorCodes, arraysize(kAllNetErrorCodes)));
  }

  // Destroy the ResourceLoader.
  RemovePendingRequest(info->GetChildID(), info->GetRequestID());
}

// static
bool ResourceDispatcherHostImpl::RenderViewForRequest(
    const net::URLRequest* request,
    int* render_process_id,
    int* render_view_id) {
  const ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request);
  if (!info) {
    *render_process_id = -1;
    *render_view_id = -1;
    return false;
  }

  return info->GetAssociatedRenderView(render_process_id, render_view_id);
}

void ResourceDispatcherHostImpl::OnShutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  is_shutdown_ = true;
  pending_loaders_.clear();

  // Make sure we shutdown the timer now, otherwise by the time our destructor
  // runs if the timer is still running the Task is deleted twice (once by
  // the MessageLoop and the second time by RepeatingTimer).
  update_load_states_timer_.reset();

  // Clear blocked requests if any left.
  // Note that we have to do this in 2 passes as we cannot call
  // CancelBlockedRequestsForRoute while iterating over
  // blocked_loaders_map_, as it modifies it.
  std::set<ProcessRouteIDs> ids;
  for (BlockedLoadersMap::const_iterator iter = blocked_loaders_map_.begin();
       iter != blocked_loaders_map_.end(); ++iter) {
    std::pair<std::set<ProcessRouteIDs>::iterator, bool> result =
        ids.insert(iter->first);
    // We should not have duplicates.
    DCHECK(result.second);
  }
  for (std::set<ProcessRouteIDs>::const_iterator iter = ids.begin();
       iter != ids.end(); ++iter) {
    CancelBlockedRequestsForRoute(iter->first, iter->second);
  }
}

bool ResourceDispatcherHostImpl::OnMessageReceived(
    const IPC::Message& message,
    ResourceMessageFilter* filter,
    bool* message_was_ok) {
  filter_ = filter;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ResourceDispatcherHostImpl, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_RequestResource, OnRequestResource)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ResourceHostMsg_SyncLoad, OnSyncLoad)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_ReleaseDownloadedFile,
                        OnReleaseDownloadedFile)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_DataDownloaded_ACK, OnDataDownloadedACK)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_UploadProgress_ACK, OnUploadProgressACK)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_CancelRequest, OnCancelRequest)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SwapOut_ACK, OnSwapOutACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidLoadResourceFromMemoryCache,
                        OnDidLoadResourceFromMemoryCache)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  if (!handled && IPC_MESSAGE_ID_CLASS(message.type()) == ResourceMsgStart) {
    PickleIterator iter(message);
    int request_id = -1;
    bool ok = iter.ReadInt(&request_id);
    DCHECK(ok);
    GlobalRequestID id(filter_->child_id(), request_id);
    DelegateMap::iterator it = delegate_map_.find(id);
    if (it != delegate_map_.end()) {
      ObserverList<ResourceMessageDelegate>::Iterator del_it(*it->second);
      ResourceMessageDelegate* delegate;
      while (!handled && (delegate = del_it.GetNext()) != NULL) {
        handled = delegate->OnMessageReceived(message, message_was_ok);
      }
    }
  }

  if (message.type() == ViewHostMsg_DidLoadResourceFromMemoryCache::ID) {
    // We just needed to peek at this message. We still want it to reach its
    // normal destination.
    handled = false;
  }

  filter_ = NULL;
  return handled;
}

void ResourceDispatcherHostImpl::OnRequestResource(
    const IPC::Message& message,
    int request_id,
    const ResourceHostMsg_Request& request_data) {
  BeginRequest(request_id, request_data, NULL, message.routing_id());
}

// Begins a resource request with the given params on behalf of the specified
// child process.  Responses will be dispatched through the given receiver. The
// process ID is used to lookup WebContentsImpl from routing_id's in the case of
// a request from a renderer.  request_context is the cookie/cache context to be
// used for this request.
//
// If sync_result is non-null, then a SyncLoad reply will be generated, else
// a normal asynchronous set of response messages will be generated.
void ResourceDispatcherHostImpl::OnSyncLoad(
    int request_id,
    const ResourceHostMsg_Request& request_data,
    IPC::Message* sync_result) {
  BeginRequest(request_id, request_data, sync_result,
               sync_result->routing_id());
}

void ResourceDispatcherHostImpl::BeginRequest(
    int request_id,
    const ResourceHostMsg_Request& request_data,
    IPC::Message* sync_result,  // only valid for sync
    int route_id) {
  ProcessType process_type = filter_->process_type();
  int child_id = filter_->child_id();

  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/91398
  char url_buf[128];
  base::strlcpy(url_buf, request_data.url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);

  // If the request that's coming in is being transferred from another process,
  // we want to reuse and resume the old loader rather than start a new one.
  linked_ptr<ResourceLoader> deferred_loader;
  {
    LoaderMap::iterator it = pending_loaders_.find(
        GlobalRequestID(request_data.transferred_request_child_id,
                        request_data.transferred_request_request_id));
    if (it != pending_loaders_.end()) {
      if (it->second->is_transferring()) {
        deferred_loader = it->second;
        pending_loaders_.erase(it);
      } else {
        RecordAction(UserMetricsAction("BadMessageTerminate_RDH"));
        filter_->BadMessageReceived();
        return;
      }
    }
  }

  ResourceContext* resource_context = filter_->resource_context();
  // http://crbug.com/90971
  CHECK(ContainsKey(active_resource_contexts_, resource_context));

  if (is_shutdown_ ||
      !ShouldServiceRequest(process_type, child_id, request_data)) {
    AbortRequestBeforeItStarts(filter_, sync_result, route_id, request_id);
    return;
  }

  const Referrer referrer(MaybeStripReferrer(request_data.referrer),
                          request_data.referrer_policy);

  // Allow the observer to block/handle the request.
  if (delegate_ && !delegate_->ShouldBeginRequest(child_id,
                                                  route_id,
                                                  request_data.method,
                                                  request_data.url,
                                                  request_data.resource_type,
                                                  resource_context,
                                                  referrer)) {
    AbortRequestBeforeItStarts(filter_, sync_result, route_id, request_id);
    return;
  }

  bool is_sync_load = sync_result != NULL;
  int load_flags =
      BuildLoadFlagsForRequest(request_data, child_id, is_sync_load);

  // Construct the request.
  scoped_ptr<net::URLRequest> new_request;
  net::URLRequest* request;
  if (deferred_loader.get()) {
    request = deferred_loader->request();

    // Give the ResourceLoader (or any of the ResourceHandlers held by it) a
    // chance to reset some state before we complete the transfer.
    deferred_loader->WillCompleteTransfer();
  } else {
    net::URLRequestContext* context =
        filter_->GetURLRequestContext(request_data.resource_type);
    new_request.reset(context->CreateRequest(request_data.url, NULL));
    request = new_request.get();

    request->set_method(request_data.method);
    request->set_first_party_for_cookies(request_data.first_party_for_cookies);
    request->set_referrer(referrer.url.spec());
    webkit_glue::ConfigureURLRequestForReferrerPolicy(request,
                                                      referrer.policy);
    net::HttpRequestHeaders headers;
    headers.AddHeadersFromString(request_data.headers);
    request->SetExtraRequestHeaders(headers);
  }

  // TODO(darin): Do we really need all of these URLRequest setters in the
  // transferred navigation case?

  request->set_load_flags(load_flags);

  request->set_priority(DetermineRequestPriority(request_data));

  // Resolve elements from request_body and prepare upload data.
  if (request_data.request_body) {
    request->set_upload(make_scoped_ptr(
        request_data.request_body->ResolveElementsAndCreateUploadDataStream(
            filter_->blob_storage_context()->controller(),
            filter_->file_system_context(),
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE))));
  }

  bool allow_download = request_data.allow_download &&
      ResourceType::IsFrame(request_data.resource_type);

  // Make extra info and read footer (contains request ID).
  ResourceRequestInfoImpl* extra_info =
      new ResourceRequestInfoImpl(
          process_type,
          child_id,
          route_id,
          request_data.origin_pid,
          request_id,
          request_data.is_main_frame,
          request_data.frame_id,
          request_data.parent_is_main_frame,
          request_data.parent_frame_id,
          request_data.resource_type,
          request_data.transition_type,
          false,  // is download
          allow_download,
          request_data.has_user_gesture,
          request_data.referrer_policy,
          resource_context,
          !is_sync_load);
  extra_info->AssociateWithRequest(request);  // Request takes ownership.

  if (request->url().SchemeIs(chrome::kBlobScheme)) {
    // Hang on to a reference to ensure the blob is not released prior
    // to the job being started.
    extra_info->set_requested_blob_data(
        filter_->blob_storage_context()->controller()->
            GetBlobDataFromUrl(request->url()));
  }

  // Have the appcache associate its extra info with the request.
  appcache::AppCacheInterceptor::SetExtraRequestInfo(
      request, filter_->appcache_service(), child_id,
      request_data.appcache_host_id, request_data.resource_type);

  // Construct the IPC resource handler.
  scoped_ptr<ResourceHandler> handler;
  if (sync_result) {
    handler.reset(new SyncResourceHandler(
        filter_, request, sync_result, this));
  } else {
    handler.reset(new AsyncResourceHandler(
        filter_, route_id, request, this));
  }

  // The RedirectToFileResourceHandler depends on being next in the chain.
  if (request_data.download_to_file) {
    handler.reset(
        new RedirectToFileResourceHandler(handler.Pass(), child_id, this));
  }

  // Install a CrossSiteResourceHandler if this request is coming from a
  // RenderViewHost with a pending cross-site request.  We only check this for
  // MAIN_FRAME requests. Unblock requests only come from a blocked page, do
  // not count as cross-site, otherwise it gets blocked indefinitely.
  if (request_data.resource_type == ResourceType::MAIN_FRAME &&
      process_type == PROCESS_TYPE_RENDERER &&
      CrossSiteRequestManager::GetInstance()->
          HasPendingCrossSiteRequest(child_id, route_id)) {
    // Wrap the event handler to be sure the current page's onunload handler
    // has a chance to run before we render the new page.
    handler.reset(new CrossSiteResourceHandler(handler.Pass(), child_id,
                                               route_id, request));
  }

  // Insert a buffered event handler before the actual one.
  handler.reset(
      new BufferedResourceHandler(handler.Pass(), this, request));

  ScopedVector<ResourceThrottle> throttles;
  if (delegate_) {
    bool is_continuation_of_transferred_request =
        (deferred_loader.get() != NULL);

    delegate_->RequestBeginning(request,
                                resource_context,
                                filter_->appcache_service(),
                                request_data.resource_type,
                                child_id,
                                route_id,
                                is_continuation_of_transferred_request,
                                &throttles);
  }

  if (request->has_upload()) {
    // Block power save while uploading data.
    throttles.push_back(new PowerSaveBlockResourceThrottle());
  }

  if (request_data.resource_type == ResourceType::MAIN_FRAME) {
    throttles.insert(
        throttles.begin(),
        new TransferNavigationResourceThrottle(request));
  }

  if (!throttles.empty()) {
    handler.reset(
        new ThrottlingResourceHandler(handler.Pass(), child_id, request_id,
                                      throttles.Pass()));
  }

  if (deferred_loader.get()) {
    pending_loaders_[extra_info->GetGlobalRequestID()] = deferred_loader;
    deferred_loader->CompleteTransfer(handler.Pass());
  } else {
    BeginRequestInternal(new_request.Pass(), handler.Pass());
  }
}

void ResourceDispatcherHostImpl::OnReleaseDownloadedFile(int request_id) {
  UnregisterDownloadedTempFile(filter_->child_id(), request_id);
}

void ResourceDispatcherHostImpl::OnDataDownloadedACK(int request_id) {
  // TODO(michaeln): maybe throttle DataDownloaded messages
}

void ResourceDispatcherHostImpl::RegisterDownloadedTempFile(
    int child_id, int request_id, ShareableFileReference* reference) {
  registered_temp_files_[child_id][request_id] = reference;
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
      child_id, reference->path());

  // When the temp file is deleted, revoke permissions that the renderer has
  // to that file. This covers an edge case where the file is deleted and then
  // the same name is re-used for some other purpose, we don't want the old
  // renderer to still have access to it.
  //
  // We do this when the file is deleted because the renderer can take a blob
  // reference to the temp file that outlives the url loaded that it was
  // loaded with to keep the file (and permissions) alive.
  reference->AddFinalReleaseCallback(
      base::Bind(&RemoveDownloadFileFromChildSecurityPolicy,
                 child_id));
}

void ResourceDispatcherHostImpl::UnregisterDownloadedTempFile(
    int child_id, int request_id) {
  DeletableFilesMap& map = registered_temp_files_[child_id];
  DeletableFilesMap::iterator found = map.find(request_id);
  if (found == map.end())
    return;

  map.erase(found);

  // Note that we don't remove the security bits here. This will be done
  // when all file refs are deleted (see RegisterDownloadedTempFile).
}

bool ResourceDispatcherHostImpl::Send(IPC::Message* message) {
  delete message;
  return false;
}

void ResourceDispatcherHostImpl::OnUploadProgressACK(int request_id) {
  ResourceLoader* loader = GetLoader(filter_->child_id(), request_id);
  if (loader)
    loader->OnUploadProgressACK();
}

void ResourceDispatcherHostImpl::OnCancelRequest(int request_id) {
  CancelRequest(filter_->child_id(), request_id, true);
}

ResourceRequestInfoImpl* ResourceDispatcherHostImpl::CreateRequestInfo(
    int child_id,
    int route_id,
    bool download,
    ResourceContext* context) {
  return new ResourceRequestInfoImpl(
      PROCESS_TYPE_RENDERER,
      child_id,
      route_id,
      0,
      request_id_,
      false,     // is_main_frame
      -1,        // frame_id
      false,     // parent_is_main_frame
      -1,        // parent_frame_id
      ResourceType::SUB_RESOURCE,
      PAGE_TRANSITION_LINK,
      download,  // is_download
      download,  // allow_download
      false,     // has_user_gesture
      WebKit::WebReferrerPolicyDefault,
      context,
      true);     // is_async
}


void ResourceDispatcherHostImpl::OnSwapOutACK(
  const ViewMsg_SwapOut_Params& params) {
  HandleSwapOutACK(params, false);
}

void ResourceDispatcherHostImpl::OnSimulateSwapOutACK(
    const ViewMsg_SwapOut_Params& params) {
  // Call the real implementation with true, which means that we timed out.
  HandleSwapOutACK(params, true);
}

void ResourceDispatcherHostImpl::HandleSwapOutACK(
    const ViewMsg_SwapOut_Params& params, bool timed_out) {
  // Closes for cross-site transitions are handled such that the cross-site
  // transition continues.
  ResourceLoader* loader = GetLoader(params.new_render_process_host_id,
                                     params.new_request_id);
  if (loader) {
    // The response we were meant to resume could have already been canceled.
    ResourceRequestInfoImpl* info = loader->GetRequestInfo();
    if (info->cross_site_handler())
      info->cross_site_handler()->ResumeResponse();
  }

  // Update the RenderViewHost's internal state after the ACK.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OnSwapOutACKHelper,
                 params.closing_process_id,
                 params.closing_route_id,
                 timed_out));
}

void ResourceDispatcherHostImpl::OnDidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& security_info,
    const std::string& http_method,
    const std::string& mime_type,
    ResourceType::Type resource_type) {
  if (!url.is_valid() || !(url.SchemeIs("http") || url.SchemeIs("https")))
    return;

  filter_->GetURLRequestContext(resource_type)->http_transaction_factory()->
      GetCache()->OnExternalCacheHit(url, http_method);
}

// This function is only used for saving feature.
void ResourceDispatcherHostImpl::BeginSaveFile(
    const GURL& url,
    const Referrer& referrer,
    int child_id,
    int route_id,
    ResourceContext* context) {
  if (is_shutdown_)
    return;

  // http://crbug.com/90971
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);
  CHECK(ContainsKey(active_resource_contexts_, context));

  scoped_ptr<ResourceHandler> handler(
      new SaveFileResourceHandler(child_id,
                                  route_id,
                                  url,
                                  save_file_manager_.get()));
  request_id_--;

  const net::URLRequestContext* request_context = context->GetRequestContext();
  bool known_proto =
      request_context->job_factory()->IsHandledURL(url);
  if (!known_proto) {
    // Since any URLs which have non-standard scheme have been filtered
    // by save manager(see GURL::SchemeIsStandard). This situation
    // should not happen.
    NOTREACHED();
    return;
  }

  scoped_ptr<net::URLRequest> request(
      request_context->CreateRequest(url, NULL));
  request->set_method("GET");
  request->set_referrer(MaybeStripReferrer(referrer.url).spec());
  webkit_glue::ConfigureURLRequestForReferrerPolicy(request.get(),
                                                    referrer.policy);
  // So far, for saving page, we need fetch content from cache, in the
  // future, maybe we can use a configuration to configure this behavior.
  request->set_load_flags(net::LOAD_PREFERRING_CACHE);

  // Since we're just saving some resources we need, disallow downloading.
  ResourceRequestInfoImpl* extra_info =
      CreateRequestInfo(child_id, route_id, false, context);
  extra_info->AssociateWithRequest(request.get());  // Request takes ownership.

  BeginRequestInternal(request.Pass(), handler.Pass());
}

void ResourceDispatcherHostImpl::MarkAsTransferredNavigation(
    const GlobalRequestID& id) {
  GetLoader(id)->MarkAsTransferring();
}

int ResourceDispatcherHostImpl::GetOutstandingRequestsMemoryCost(
    int child_id) const {
  OutstandingRequestsMemoryCostMap::const_iterator entry =
      outstanding_requests_memory_cost_map_.find(child_id);
  return (entry == outstanding_requests_memory_cost_map_.end()) ?
      0 : entry->second;
}

// The object died, so cancel and detach all requests associated with it except
// for downloads, which belong to the browser process even if initiated via a
// renderer.
void ResourceDispatcherHostImpl::CancelRequestsForProcess(int child_id) {
  CancelRequestsForRoute(child_id, -1 /* cancel all */);
  registered_temp_files_.erase(child_id);
}

void ResourceDispatcherHostImpl::CancelRequestsForRoute(int child_id,
                                                        int route_id) {
  // Since pending_requests_ is a map, we first build up a list of all of the
  // matching requests to be cancelled, and then we cancel them.  Since there
  // may be more than one request to cancel, we cannot simply hold onto the map
  // iterators found in the first loop.

  // Find the global ID of all matching elements.
  std::vector<GlobalRequestID> matching_requests;
  for (LoaderMap::const_iterator i = pending_loaders_.begin();
       i != pending_loaders_.end(); ++i) {
    if (i->first.child_id != child_id)
      continue;

    ResourceRequestInfoImpl* info = i->second->GetRequestInfo();

    GlobalRequestID id(child_id, i->first.request_id);
    DCHECK(id == i->first);

    // Don't cancel navigations that are transferring to another process,
    // since they belong to another process now.
    if (!info->is_download() && !IsTransferredNavigation(id) &&
        (route_id == -1 || route_id == info->GetRouteID())) {
      matching_requests.push_back(id);
    }
  }

  // Remove matches.
  for (size_t i = 0; i < matching_requests.size(); ++i) {
    LoaderMap::iterator iter = pending_loaders_.find(matching_requests[i]);
    // Although every matching request was in pending_requests_ when we built
    // matching_requests, it is normal for a matching request to be not found
    // in pending_requests_ after we have removed some matching requests from
    // pending_requests_.  For example, deleting a net::URLRequest that has
    // exclusive (write) access to an HTTP cache entry may unblock another
    // net::URLRequest that needs exclusive access to the same cache entry, and
    // that net::URLRequest may complete and remove itself from
    // pending_requests_. So we need to check that iter is not equal to
    // pending_requests_.end().
    if (iter != pending_loaders_.end())
      RemovePendingLoader(iter);
  }

  // Now deal with blocked requests if any.
  if (route_id != -1) {
    if (blocked_loaders_map_.find(ProcessRouteIDs(child_id, route_id)) !=
        blocked_loaders_map_.end()) {
      CancelBlockedRequestsForRoute(child_id, route_id);
    }
  } else {
    // We have to do all render views for the process |child_id|.
    // Note that we have to do this in 2 passes as we cannot call
    // CancelBlockedRequestsForRoute while iterating over
    // blocked_loaders_map_, as it modifies it.
    std::set<int> route_ids;
    for (BlockedLoadersMap::const_iterator iter = blocked_loaders_map_.begin();
         iter != blocked_loaders_map_.end(); ++iter) {
      if (iter->first.first == child_id)
        route_ids.insert(iter->first.second);
    }
    for (std::set<int>::const_iterator iter = route_ids.begin();
        iter != route_ids.end(); ++iter) {
      CancelBlockedRequestsForRoute(child_id, *iter);
    }
  }
}

// Cancels the request and removes it from the list.
void ResourceDispatcherHostImpl::RemovePendingRequest(int child_id,
                                                      int request_id) {
  LoaderMap::iterator i = pending_loaders_.find(
      GlobalRequestID(child_id, request_id));
  if (i == pending_loaders_.end()) {
    NOTREACHED() << "Trying to remove a request that's not here";
    return;
  }
  RemovePendingLoader(i);
}

void ResourceDispatcherHostImpl::RemovePendingLoader(
    const LoaderMap::iterator& iter) {
  ResourceRequestInfoImpl* info = iter->second->GetRequestInfo();

  // Remove the memory credit that we added when pushing the request onto
  // the pending list.
  IncrementOutstandingRequestsMemoryCost(-1 * info->memory_cost(),
                                         info->GetChildID());

  pending_loaders_.erase(iter);

  // If we have no more pending requests, then stop the load state monitor
  if (pending_loaders_.empty() && update_load_states_timer_.get())
    update_load_states_timer_->Stop();
}

void ResourceDispatcherHostImpl::CancelRequest(int child_id,
                                               int request_id,
                                               bool from_renderer) {
  if (from_renderer) {
    // When the old renderer dies, it sends a message to us to cancel its
    // requests.
    if (IsTransferredNavigation(GlobalRequestID(child_id, request_id)))
      return;
  }

  ResourceLoader* loader = GetLoader(child_id, request_id);
  if (!loader) {
    // We probably want to remove this warning eventually, but I wanted to be
    // able to notice when this happens during initial development since it
    // should be rare and may indicate a bug.
    DVLOG(1) << "Canceling a request that wasn't found";
    return;
  }

  loader->CancelRequest(from_renderer);
}

int ResourceDispatcherHostImpl::IncrementOutstandingRequestsMemoryCost(
    int cost,
    int child_id) {
  // Retrieve the previous value (defaulting to 0 if not found).
  OutstandingRequestsMemoryCostMap::iterator prev_entry =
      outstanding_requests_memory_cost_map_.find(child_id);
  int new_cost = 0;
  if (prev_entry != outstanding_requests_memory_cost_map_.end())
    new_cost = prev_entry->second;

  // Insert/update the total; delete entries when their value reaches 0.
  new_cost += cost;
  CHECK(new_cost >= 0);
  if (new_cost == 0)
    outstanding_requests_memory_cost_map_.erase(child_id);
  else
    outstanding_requests_memory_cost_map_[child_id] = new_cost;

  return new_cost;
}

// static
int ResourceDispatcherHostImpl::CalculateApproximateMemoryCost(
    net::URLRequest* request) {
  // The following fields should be a minor size contribution (experimentally
  // on the order of 100). However since they are variable length, it could
  // in theory be a sizeable contribution.
  int strings_cost = request->extra_request_headers().ToString().size() +
                     request->original_url().spec().size() +
                     request->referrer().size() +
                     request->method().size();

  // Note that this expression will typically be dominated by:
  // |kAvgBytesPerOutstandingRequest|.
  return kAvgBytesPerOutstandingRequest + strings_cost;
}

void ResourceDispatcherHostImpl::BeginRequestInternal(
    scoped_ptr<net::URLRequest> request,
    scoped_ptr<ResourceHandler> handler) {
  DCHECK(!request->is_pending());
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request.get());

  if ((TimeTicks::Now() - last_user_gesture_time_) <
      TimeDelta::FromMilliseconds(kUserGestureWindowMs)) {
    request->set_load_flags(
        request->load_flags() | net::LOAD_MAYBE_USER_GESTURE);
  }

  // Add the memory estimate that starting this request will consume.
  info->set_memory_cost(CalculateApproximateMemoryCost(request.get()));
  int memory_cost = IncrementOutstandingRequestsMemoryCost(info->memory_cost(),
                                                           info->GetChildID());

  // If enqueing/starting this request will exceed our per-process memory
  // bound, abort it right away.
  if (memory_cost > max_outstanding_requests_cost_per_process_) {
    // We call "CancelWithError()" as a way of setting the net::URLRequest's
    // status -- it has no effect beyond this, since the request hasn't started.
    request->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);

    if (!handler->OnResponseCompleted(info->GetRequestID(), request->status(),
                                      std::string())) {
      // TODO(darin): The handler is not ready for us to kill the request. Oops!
      NOTREACHED();
    }

    IncrementOutstandingRequestsMemoryCost(-1 * info->memory_cost(),
                                           info->GetChildID());

    // A ResourceHandler must not outlive its associated URLRequest.
    handler.reset();
    return;
  }

  linked_ptr<ResourceLoader> loader(
      new ResourceLoader(request.Pass(), handler.Pass(), this));

  ProcessRouteIDs pair_id(info->GetChildID(), info->GetRouteID());
  BlockedLoadersMap::const_iterator iter = blocked_loaders_map_.find(pair_id);
  if (iter != blocked_loaders_map_.end()) {
    // The request should be blocked.
    iter->second->push_back(loader);
    return;
  }

  StartLoading(info, loader);
}

void ResourceDispatcherHostImpl::StartLoading(
    ResourceRequestInfoImpl* info,
    const linked_ptr<ResourceLoader>& loader) {
  pending_loaders_[info->GetGlobalRequestID()] = loader;

  loader->StartRequest();
}

void ResourceDispatcherHostImpl::OnUserGesture(WebContentsImpl* contents) {
  last_user_gesture_time_ = TimeTicks::Now();
}

net::URLRequest* ResourceDispatcherHostImpl::GetURLRequest(
    const GlobalRequestID& id) {
  ResourceLoader* loader = GetLoader(id);
  if (!loader)
    return NULL;

  return loader->request();
}

namespace {

// This function attempts to return the "more interesting" load state of |a|
// and |b|.  We don't have temporal information about these load states
// (meaning we don't know when we transitioned into these states), so we just
// rank them according to how "interesting" the states are.
//
// We take advantage of the fact that the load states are an enumeration listed
// in the order in which they occur during the lifetime of a request, so we can
// regard states with larger numeric values as being further along toward
// completion.  We regard those states as more interesting to report since they
// represent progress.
//
// For example, by this measure "tranferring data" is a more interesting state
// than "resolving host" because when we are transferring data we are actually
// doing something that corresponds to changes that the user might observe,
// whereas waiting for a host name to resolve implies being stuck.
//
const net::LoadStateWithParam& MoreInterestingLoadState(
    const net::LoadStateWithParam& a, const net::LoadStateWithParam& b) {
  return (a.state < b.state) ? b : a;
}

// Carries information about a load state change.
struct LoadInfo {
  GURL url;
  net::LoadStateWithParam load_state;
  uint64 upload_position;
  uint64 upload_size;
};

// Map from ProcessID+ViewID pair to LoadState
typedef std::map<std::pair<int, int>, LoadInfo> LoadInfoMap;

// Used to marshal calls to LoadStateChanged from the IO to UI threads.  We do
// them all as a single callback to avoid spamming the UI thread.
void LoadInfoUpdateCallback(const LoadInfoMap& info_map) {
  LoadInfoMap::const_iterator i;
  for (i = info_map.begin(); i != info_map.end(); ++i) {
    RenderViewHostImpl* view =
        RenderViewHostImpl::FromID(i->first.first, i->first.second);
    if (view)  // The view could be gone at this point.
      view->LoadStateChanged(i->second.url, i->second.load_state,
                             i->second.upload_position,
                             i->second.upload_size);
  }
}

}  // namespace

void ResourceDispatcherHostImpl::UpdateLoadStates() {
  // Populate this map with load state changes, and then send them on to the UI
  // thread where they can be passed along to the respective RVHs.
  LoadInfoMap info_map;

  LoaderMap::const_iterator i;

  // Determine the largest upload size of all requests
  // in each View (good chance it's zero).
  std::map<std::pair<int, int>, uint64> largest_upload_size;
  for (i = pending_loaders_.begin(); i != pending_loaders_.end(); ++i) {
    net::URLRequest* request = i->second->request();
    ResourceRequestInfoImpl* info = i->second->GetRequestInfo();
    uint64 upload_size = request->GetUploadProgress().size();
    if (request->GetLoadState().state != net::LOAD_STATE_SENDING_REQUEST)
      upload_size = 0;
    std::pair<int, int> key(info->GetChildID(), info->GetRouteID());
    if (upload_size && largest_upload_size[key] < upload_size)
      largest_upload_size[key] = upload_size;
  }

  for (i = pending_loaders_.begin(); i != pending_loaders_.end(); ++i) {
    net::URLRequest* request = i->second->request();
    ResourceRequestInfoImpl* info = i->second->GetRequestInfo();
    net::LoadStateWithParam load_state = request->GetLoadState();
    net::UploadProgress progress = request->GetUploadProgress();

    // We also poll for upload progress on this timer and send upload
    // progress ipc messages to the plugin process.
    i->second->ReportUploadProgress();

    std::pair<int, int> key(info->GetChildID(), info->GetRouteID());

    // If a request is uploading data, ignore all other requests so that the
    // upload progress takes priority for being shown in the status bar.
    if (largest_upload_size.find(key) != largest_upload_size.end() &&
        progress.size() < largest_upload_size[key])
      continue;

    net::LoadStateWithParam to_insert = load_state;
    LoadInfoMap::iterator existing = info_map.find(key);
    if (existing != info_map.end()) {
      to_insert =
          MoreInterestingLoadState(existing->second.load_state, load_state);
      if (to_insert.state == existing->second.load_state.state)
        continue;
    }
    LoadInfo& load_info = info_map[key];
    load_info.url = request->url();
    load_info.load_state = to_insert;
    load_info.upload_size = progress.size();
    load_info.upload_position = progress.position();
  }

  if (info_map.empty())
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&LoadInfoUpdateCallback, info_map));
}

void ResourceDispatcherHostImpl::BlockRequestsForRoute(int child_id,
                                                       int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProcessRouteIDs key(child_id, route_id);
  DCHECK(blocked_loaders_map_.find(key) == blocked_loaders_map_.end()) <<
      "BlockRequestsForRoute called  multiple time for the same RVH";
  blocked_loaders_map_[key] = new BlockedLoadersList();
}

void ResourceDispatcherHostImpl::ResumeBlockedRequestsForRoute(int child_id,
                                                               int route_id) {
  ProcessBlockedRequestsForRoute(child_id, route_id, false);
}

void ResourceDispatcherHostImpl::CancelBlockedRequestsForRoute(int child_id,
                                                               int route_id) {
  ProcessBlockedRequestsForRoute(child_id, route_id, true);
}

void ResourceDispatcherHostImpl::ProcessBlockedRequestsForRoute(
    int child_id,
    int route_id,
    bool cancel_requests) {
  BlockedLoadersMap::iterator iter = blocked_loaders_map_.find(
      std::pair<int, int>(child_id, route_id));
  if (iter == blocked_loaders_map_.end()) {
    // It's possible to reach here if the renderer crashed while an interstitial
    // page was showing.
    return;
  }

  BlockedLoadersList* loaders = iter->second;

  // Removing the vector from the map unblocks any subsequent requests.
  blocked_loaders_map_.erase(iter);

  for (BlockedLoadersList::iterator loaders_iter = loaders->begin();
       loaders_iter != loaders->end(); ++loaders_iter) {
    linked_ptr<ResourceLoader> loader = *loaders_iter;
    ResourceRequestInfoImpl* info = loader->GetRequestInfo();
    if (cancel_requests) {
      IncrementOutstandingRequestsMemoryCost(-1 * info->memory_cost(),
                                             info->GetChildID());
    } else {
      StartLoading(info, loader);
    }
  }

  delete loaders;
}

ResourceDispatcherHostImpl::HttpAuthResourceType
ResourceDispatcherHostImpl::HttpAuthResourceTypeOf(net::URLRequest* request) {
  // Use the same critera as for cookies to determine the sub-resource type
  // that is requesting to be authenticated.
  if (!request->first_party_for_cookies().is_valid())
    return HTTP_AUTH_RESOURCE_TOP;

  if (net::RegistryControlledDomainService::SameDomainOrHost(
          request->first_party_for_cookies(), request->url()))
    return HTTP_AUTH_RESOURCE_SAME_DOMAIN;

  if (allow_cross_origin_auth_prompt())
    return HTTP_AUTH_RESOURCE_ALLOWED_CROSS;

  return HTTP_AUTH_RESOURCE_BLOCKED_CROSS;
}

bool ResourceDispatcherHostImpl::allow_cross_origin_auth_prompt() {
  return allow_cross_origin_auth_prompt_;
}

bool ResourceDispatcherHostImpl::IsTransferredNavigation(
    const GlobalRequestID& id) const {
  ResourceLoader* loader = GetLoader(id);
  return loader ? loader->is_transferring() : false;
}

ResourceLoader* ResourceDispatcherHostImpl::GetLoader(
    const GlobalRequestID& id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  LoaderMap::const_iterator i = pending_loaders_.find(id);
  if (i == pending_loaders_.end())
    return NULL;

  return i->second.get();
}

ResourceLoader* ResourceDispatcherHostImpl::GetLoader(int child_id,
                                                      int request_id) const {
  return GetLoader(GlobalRequestID(child_id, request_id));
}

void ResourceDispatcherHostImpl::RegisterResourceMessageDelegate(
    const GlobalRequestID& id, ResourceMessageDelegate* delegate) {
  DelegateMap::iterator it = delegate_map_.find(id);
  if (it == delegate_map_.end()) {
    it = delegate_map_.insert(
        std::make_pair(id, new ObserverList<ResourceMessageDelegate>)).first;
  }
  it->second->AddObserver(delegate);
}

void ResourceDispatcherHostImpl::UnregisterResourceMessageDelegate(
    const GlobalRequestID& id, ResourceMessageDelegate* delegate) {
  DCHECK(ContainsKey(delegate_map_, id));
  DelegateMap::iterator it = delegate_map_.find(id);
  DCHECK(it->second->HasObserver(delegate));
  it->second->RemoveObserver(delegate);
  if (it->second->size() == 0) {
    delete it->second;
    delegate_map_.erase(it);
  }
}

}  // namespace content
