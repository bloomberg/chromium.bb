// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"

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
#include "base/threading/thread_restrictions.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/cert_store_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/cross_site_request_manager.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/download/save_file_resource_handler.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/async_resource_handler.h"
#include "content/browser/renderer_host/buffered_resource_handler.h"
#include "content/browser/renderer_host/cross_site_resource_handler.h"
#include "content/browser/renderer_host/doomed_resource_handler.h"
#include "content/browser/renderer_host/redirect_to_file_resource_handler.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "content/public/browser/resource_request_details.h"
#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "content/browser/renderer_host/sync_resource_handler.h"
#include "content/browser/renderer_host/throttling_resource_handler.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "content/common/resource_messages.h"
#include "content/common/ssl_status_serialization.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/url_constants.h"
#include "net/base/auth.h"
#include "net/base/cert_status_flags.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domain.h"
#include "net/base/request_priority.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/upload_data.h"
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
#include "webkit/glue/webkit_glue.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using webkit_blob::ShareableFileReference;

// ----------------------------------------------------------------------------

namespace content {

namespace {

static ResourceDispatcherHostImpl* g_resource_dispatcher_host;

// The interval for calls to ResourceDispatcherHostImpl::UpdateLoadStates
const int kUpdateLoadStatesIntervalMsec = 100;

// Maximum number of pending data messages sent to the renderer at any
// given time for a given request.
const int kMaxPendingDataMessages = 20;

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
  net::URLRequestStatus status(net::URLRequestStatus::FAILED,
                               net::ERR_ABORTED);
  if (sync_result) {
    SyncLoadResult result;
    result.status = status;
    ResourceHostMsg_SyncLoad::WriteReplyParams(sync_result, result);
    filter->Send(sync_result);
  } else {
    // Tell the renderer that this request was disallowed.
    filter->Send(new ResourceMsg_RequestComplete(
        route_id,
        request_id,
        status,
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
  if (request_data.upload_data) {
    const std::vector<net::UploadData::Element>* uploads =
        request_data.upload_data->elements();
    std::vector<net::UploadData::Element>::const_iterator iter;
    for (iter = uploads->begin(); iter != uploads->end(); ++iter) {
      if (iter->type() == net::UploadData::TYPE_FILE &&
          !policy->CanReadFile(child_id, iter->file_path())) {
        NOTREACHED() << "Denied unauthorized upload of "
                     << iter->file_path().value();
        return false;
      }
    }
  }

  return true;
}

void PopulateResourceResponse(net::URLRequest* request,
                              ResourceResponse* response) {
  response->status = request->status();
  response->request_time = request->request_time();
  response->response_time = request->response_time();
  response->headers = request->response_headers();
  request->GetCharset(&response->charset);
  response->content_length = request->GetExpectedContentSize();
  request->GetMimeType(&response->mime_type);
  net::HttpResponseInfo response_info = request->response_info();
  response->was_fetched_via_spdy = response_info.was_fetched_via_spdy;
  response->was_npn_negotiated = response_info.was_npn_negotiated;
  response->npn_negotiated_protocol = response_info.npn_negotiated_protocol;
  response->was_fetched_via_proxy = request->was_fetched_via_proxy();
  response->socket_address = request->GetSocketAddress();
  appcache::AppCacheInterceptor::GetExtraResponseInfo(
      request,
      &response->appcache_id,
      &response->appcache_manifest_url);
}

void RemoveDownloadFileFromChildSecurityPolicy(int child_id,
                                               const FilePath& path) {
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

net::RequestPriority DetermineRequestPriority(ResourceType::Type type) {
  // Determine request priority based on how critical this resource typically
  // is to user-perceived page load performance. Important considerations are:
  // * Can this resource block the download of other resources.
  // * Can this resource block the rendering of the page.
  // * How useful is the page to the user if this resource is not loaded yet.

  switch (type) {
    // Main frames are the highest priority because they can block nearly every
    // type of other resource and there is no useful display without them.
    // Sub frames are a close second, however it is a common pattern to wrap
    // ads in an iframe or even in multiple nested iframes. It is worth
    // investigating if there is a better priority for them.
    case ResourceType::MAIN_FRAME:
    case ResourceType::SUB_FRAME:
      return net::HIGHEST;

    // Stylesheets and scripts can block rendering and loading of other
    // resources. Fonts can block text from rendering.
    case ResourceType::STYLESHEET:
    case ResourceType::SCRIPT:
    case ResourceType::FONT_RESOURCE:
      return net::MEDIUM;

    // Sub resources, objects and media are lower priority than potentially
    // blocking stylesheets, scripts and fonts, but are higher priority than
    // images because if they exist they are probably more central to the page
    // focus than images on the page.
    case ResourceType::SUB_RESOURCE:
    case ResourceType::OBJECT:
    case ResourceType::MEDIA:
    case ResourceType::WORKER:
    case ResourceType::SHARED_WORKER:
    case ResourceType::XHR:
      return net::LOW;

    // Images are the "lowest" priority because they typically do not block
    // downloads or rendering and most pages have some useful content without
    // them.
    case ResourceType::IMAGE:
    // Favicons aren't required for rendering the current page, but
    // are user visible.
    case ResourceType::FAVICON:
      return net::LOWEST;

    // Prefetches and prerenders are at a lower priority than even
    // LOWEST, since they are not even required for rendering of the
    // current page.
    case ResourceType::PREFETCH:
    case ResourceType::PRERENDER:
      return net::IDLE;

    default:
      // When new resource types are added, their priority must be considered.
      NOTREACHED();
      return net::LOW;
  }
}

void OnSwapOutACKHelper(int render_process_id, int render_view_id) {
  RenderViewHostImpl* rvh = RenderViewHostImpl::FromID(render_process_id,
                                                       render_view_id);
  if (rvh)
    rvh->OnSwapOutACK();
}

net::Error CallbackAndReturn(
    const DownloadResourceHandler::OnStartedCallback& started_cb,
    net::Error net_error) {
  if (started_cb.is_null())
    return net_error;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(started_cb, content::DownloadId::Invalid(), net_error));

  return net_error;
}

}  // namespace

// static
ResourceDispatcherHost* ResourceDispatcherHost::Get() {
  return g_resource_dispatcher_host;
}

ResourceDispatcherHostImpl::ResourceDispatcherHostImpl()
    : download_file_manager_(new DownloadFileManager(NULL)),
      save_file_manager_(new SaveFileManager()),
      request_id_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
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
  AsyncResourceHandler::GlobalCleanup();
  for (PendingRequestList::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    transferred_navigations_.erase(i->first);
  }
  STLDeleteValues(&pending_requests_);
  DCHECK(transferred_navigations_.empty());
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

void ResourceDispatcherHostImpl::CancelRequestsForContext(
    ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(context);

  canceled_resource_contexts_.insert(context);

  // Note that request cancellation has side effects. Therefore, we gather all
  // the requests to cancel first, and then we start cancelling. We assert at
  // the end that there are no more to cancel since the context is about to go
  // away.
  std::vector<net::URLRequest*> requests_to_cancel;
  for (PendingRequestList::iterator i = pending_requests_.begin();
       i != pending_requests_.end();) {
    ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(i->second);
    if (info->GetContext() == context) {
      requests_to_cancel.push_back(i->second);
      pending_requests_.erase(i++);
    } else {
      ++i;
    }
  }

  for (BlockedRequestMap::iterator i = blocked_requests_map_.begin();
       i != blocked_requests_map_.end();) {
    BlockedRequestsList* requests = i->second;
    if (requests->empty()) {
      // This can happen if BlockRequestsForRoute() has been called for a route,
      // but we haven't blocked any matching requests yet.
      ++i;
      continue;
    }
    ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(requests->front());
    if (info->GetContext() == context) {
      blocked_requests_map_.erase(i++);
      for (BlockedRequestsList::const_iterator it = requests->begin();
           it != requests->end(); ++it) {
        net::URLRequest* request = *it;
        info = ResourceRequestInfoImpl::ForRequest(request);
        // We make the assumption that all requests on the list have the same
        // ResourceContext.
        DCHECK_EQ(context, info->GetContext());
        IncrementOutstandingRequestsMemoryCost(-1 * info->memory_cost(),
                                               info->GetChildID());
        requests_to_cancel.push_back(request);
      }
      delete requests;
    } else {
      ++i;
    }
  }

  for (std::vector<net::URLRequest*>::iterator i = requests_to_cancel.begin();
       i != requests_to_cancel.end(); ++i) {
    net::URLRequest* request = *i;
    ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(request);
    // There is no strict requirement that this be the case, but currently
    // downloads and transferred requests are the only requests that aren't
    // cancelled when the associated processes go away. It may be OK for this
    // invariant to change in the future, but if this assertion fires without
    // the invariant changing, then it's indicative of a leak.
    GlobalRequestID request_id(info->GetChildID(), info->GetRequestID());
    bool is_transferred = IsTransferredNavigation(request_id);
    DCHECK(info->is_download() || is_transferred);
    if (is_transferred)
      transferred_navigations_.erase(request_id);
    delete request;
  }

  // Validate that no more requests for this context were added.
  for (PendingRequestList::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(i->second);
    // http://crbug.com/90971
    CHECK_NE(info->GetContext(), context);
  }

  for (BlockedRequestMap::const_iterator i = blocked_requests_map_.begin();
       i != blocked_requests_map_.end(); ++i) {
    BlockedRequestsList* requests = i->second;
    if (!requests->empty()) {
      ResourceRequestInfoImpl* info =
          ResourceRequestInfoImpl::ForRequest(requests->front());
      // http://crbug.com/90971
      CHECK_NE(info->GetContext(), context);
    }
  }
}

net::Error ResourceDispatcherHostImpl::BeginDownload(
    scoped_ptr<net::URLRequest> request,
    ResourceContext* context,
    int child_id,
    int route_id,
    bool prefer_cache,
    const DownloadSaveInfo& save_info,
    const DownloadStartedCallback& started_callback) {
  if (is_shutdown_)
    return CallbackAndReturn(started_callback, net::ERR_INSUFFICIENT_RESOURCES);

  const GURL& url = request->original_url();

  // http://crbug.com/90971
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);
  CHECK(!ContainsKey(canceled_resource_contexts_, context));

  const net::URLRequestContext* request_context = context->GetRequestContext();
  request->set_referrer(MaybeStripReferrer(GURL(request->referrer())).spec());
  request->set_context(request_context);
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

  // From this point forward, the |DownloadResourceHandler| is responsible for
  // |started_callback|.
  scoped_refptr<ResourceHandler> handler(
      CreateResourceHandlerForDownload(request.get(), context, child_id,
                                       route_id, request_id_, save_info,
                                       started_callback));

  if (!request_context->job_factory()->IsHandledURL(url)) {
    VLOG(1) << "Download request for unsupported protocol: "
            << url.possibly_invalid_spec();
    return net::ERR_ACCESS_DENIED;
  }

  ResourceRequestInfoImpl* extra_info =
      CreateRequestInfo(handler, child_id, route_id, true, context);
  extra_info->AssociateWithRequest(request.get());  // Request takes ownership.

  request->set_delegate(this);
  BeginRequestInternal(request.release());

  return net::OK;
}

void ResourceDispatcherHostImpl::ClearLoginDelegateForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  if (info)
    info->set_login_delegate(NULL);
}

void ResourceDispatcherHostImpl::MarkAsTransferredNavigation(
    net::URLRequest* transferred_request) {
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(transferred_request);

  GlobalRequestID transferred_request_id(info->GetChildID(),
                                         info->GetRequestID());
  transferred_navigations_[transferred_request_id] = transferred_request;

  // If a URLRequest is transferred to a new RenderViewHost, its
  // ResourceHandler should not receive any notifications because it may
  // depend on the state of the old RVH. We set a ResourceHandler that only
  // allows canceling requests, because on shutdown of the RDH all pending
  // requests are canceled. The RVH of requests that are being transferred may
  // be gone by that time. If the request is resumed, the ResoureHandlers are
  // substituted again.
  scoped_refptr<ResourceHandler> transferred_resource_handler(
      new DoomedResourceHandler(info->resource_handler()));
  info->set_resource_handler(transferred_resource_handler.get());
}

void ResourceDispatcherHostImpl::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&ResourceDispatcherHostImpl::OnShutdown,
                                     base::Unretained(this)));
}

scoped_refptr<ResourceHandler>
ResourceDispatcherHostImpl::CreateResourceHandlerForDownload(
    net::URLRequest* request,
    ResourceContext* context,
    int child_id,
    int route_id,
    int request_id,
    const DownloadSaveInfo& save_info,
    const DownloadResourceHandler::OnStartedCallback& started_cb) {
  scoped_refptr<ResourceHandler> handler(
      new DownloadResourceHandler(child_id, route_id, request_id,
                                  request->url(), download_file_manager_.get(),
                                  request, started_cb, save_info));
  if (delegate_) {
    ScopedVector<ResourceThrottle> throttles;
    delegate_->DownloadStarting(request, context, child_id, route_id,
                                request_id, !request->is_pending(), &throttles);
    if (!throttles.empty()) {
      handler = new ThrottlingResourceHandler(this, handler, child_id,
                                              request_id, throttles.Pass());
    }
  }
  return handler;
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
  for (PendingRequestList::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    transferred_navigations_.erase(i->first);
  }
  STLDeleteValues(&pending_requests_);
  // Make sure we shutdown the timer now, otherwise by the time our destructor
  // runs if the timer is still running the Task is deleted twice (once by
  // the MessageLoop and the second time by RepeatingTimer).
  update_load_states_timer_.reset();

  // Clear blocked requests if any left.
  // Note that we have to do this in 2 passes as we cannot call
  // CancelBlockedRequestsForRoute while iterating over
  // blocked_requests_map_, as it modifies it.
  std::set<ProcessRouteIDs> ids;
  for (BlockedRequestMap::const_iterator iter = blocked_requests_map_.begin();
       iter != blocked_requests_map_.end(); ++iter) {
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

bool ResourceDispatcherHostImpl::HandleExternalProtocol(
    int request_id,
    int child_id,
    int route_id,
    const GURL& url,
    ResourceType::Type type,
    const net::URLRequestJobFactory& job_factory,
    ResourceHandler* handler) {
  if (!ResourceType::IsFrame(type) ||
      job_factory.IsHandledURL(url)) {
    return false;
  }

  if (delegate_)
    delegate_->HandleExternalProtocol(url, child_id, route_id);

  // This error code is special-cased in RenderViewImpl::didFailProvisionalLoad
  // to not result in an error page.
  handler->OnResponseCompleted(
      request_id,
      net::URLRequestStatus(net::URLRequestStatus::FAILED,
                            net::ERR_UNKNOWN_URL_SCHEME),
      std::string());  // No security info necessary.
  return true;
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
    IPC_MESSAGE_HANDLER(ResourceHostMsg_DataReceived_ACK, OnDataReceivedACK)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_DataDownloaded_ACK, OnDataDownloadedACK)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_UploadProgress_ACK, OnUploadProgressACK)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_CancelRequest, OnCancelRequest)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_TransferRequestToNewPage,
                        OnTransferRequestToNewPage)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_FollowRedirect, OnFollowRedirect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SwapOut_ACK, OnSwapOutACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidLoadResourceFromMemoryCache,
                        OnDidLoadResourceFromMemoryCache)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

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
  // we want to reuse and resume the old request rather than start a new one.
  net::URLRequest* deferred_request = NULL;

  GlobalRequestID old_request_id(request_data.transferred_request_child_id,
                                 request_data.transferred_request_request_id);
  TransferredNavigations::iterator iter =
      transferred_navigations_.find(old_request_id);
  if (iter != transferred_navigations_.end()) {
    deferred_request = iter->second;
    pending_requests_.erase(old_request_id);
    transferred_navigations_.erase(iter);
  }

  ResourceContext* resource_context = filter_->resource_context();
  // http://crbug.com/90971
  CHECK(!ContainsKey(canceled_resource_contexts_, resource_context));

  // Might need to resolve the blob references in the upload data.
  if (request_data.upload_data) {
    GetBlobStorageControllerForResourceContext(resource_context)->
        ResolveBlobReferencesInUploadData(request_data.upload_data.get());
  }

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

  // Construct the event handler.
  scoped_refptr<ResourceHandler> handler;
  if (sync_result) {
    handler = new SyncResourceHandler(
        filter_, request_data.url, sync_result, this);
  } else {
    handler = new AsyncResourceHandler(
        filter_, route_id, request_data.url, this);
  }

  // The RedirectToFileResourceHandler depends on being next in the chain.
  if (request_data.download_to_file)
    handler = new RedirectToFileResourceHandler(handler, child_id, this);

  if (HandleExternalProtocol(
      request_id, child_id, route_id, request_data.url,
      request_data.resource_type,
      *resource_context->GetRequestContext()->job_factory(), handler)) {
    return;
  }

  // Construct the request.
  net::URLRequest* request;
  if (deferred_request) {
    request = deferred_request;
  } else {
    request = new net::URLRequest(request_data.url, this);
    request->set_method(request_data.method);
    request->set_first_party_for_cookies(request_data.first_party_for_cookies);
    request->set_referrer(referrer.url.spec());
    webkit_glue::ConfigureURLRequestForReferrerPolicy(request, referrer.policy);
    net::HttpRequestHeaders headers;
    headers.AddHeadersFromString(request_data.headers);
    request->SetExtraRequestHeaders(headers);
  }

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

  if (sync_result)
    load_flags |= net::LOAD_IGNORE_LIMITS;

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanUseCookiesForOrigin(child_id, request_data.url)) {
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

  request->set_load_flags(load_flags);

  request->set_context(
      filter_->GetURLRequestContext(request_data.resource_type));
  request->set_priority(DetermineRequestPriority(request_data.resource_type));

  // Set upload data.
  uint64 upload_size = 0;
  if (request_data.upload_data) {
    request->set_upload(request_data.upload_data);
    // This results in performing file IO. crbug.com/112607.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    upload_size = request_data.upload_data->GetContentLengthSync();
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
    handler = new CrossSiteResourceHandler(handler, child_id, route_id, this);
  }

  // Insert a buffered event handler before the actual one.
  handler = new BufferedResourceHandler(handler, this, request);

  if (delegate_) {
    bool is_continuation_of_transferred_request =
        (deferred_request != NULL);

    ScopedVector<ResourceThrottle> throttles;
    delegate_->RequestBeginning(request,
                                resource_context,
                                request_data.resource_type,
                                child_id,
                                route_id,
                                is_continuation_of_transferred_request,
                                &throttles);
    if (!throttles.empty()) {
      handler = new ThrottlingResourceHandler(this, handler, child_id,
                                              request_id, throttles.Pass());
    }
  }

  bool allow_download = request_data.allow_download &&
      ResourceType::IsFrame(request_data.resource_type);
  // Make extra info and read footer (contains request ID).
  ResourceRequestInfoImpl* extra_info =
      new ResourceRequestInfoImpl(
          handler,
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
          upload_size,
          false,  // is download
          allow_download,
          request_data.has_user_gesture,
          request_data.referrer_policy,
          resource_context);
  extra_info->AssociateWithRequest(request);  // Request takes ownership.

  if (request->url().SchemeIs(chrome::kBlobScheme)) {
    // Hang on to a reference to ensure the blob is not released prior
    // to the job being started.
    webkit_blob::BlobStorageController* controller =
        GetBlobStorageControllerForResourceContext(resource_context);
    extra_info->set_requested_blob_data(
        controller->GetBlobDataFromUrl(request->url()));
  }

  // Have the appcache associate its extra info with the request.
  appcache::AppCacheInterceptor::SetExtraRequestInfo(
      request, ResourceContext::GetAppCacheService(resource_context), child_id,
      request_data.appcache_host_id, request_data.resource_type);

  if (deferred_request) {
    // This is a request that has been transferred from another process, so
    // resume it rather than continuing the regular procedure for starting a
    // request. Currently this is only done for redirects.
    GlobalRequestID global_id(extra_info->GetChildID(),
                              extra_info->GetRequestID());
    pending_requests_[global_id] = request;
    request->FollowDeferredRedirect();
  } else {
    BeginRequestInternal(request);
  }
}

void ResourceDispatcherHostImpl::OnReleaseDownloadedFile(int request_id) {
  DCHECK(pending_requests_.end() ==
         pending_requests_.find(
             GlobalRequestID(filter_->child_id(), request_id)));
  UnregisterDownloadedTempFile(filter_->child_id(), request_id);
}

void ResourceDispatcherHostImpl::OnDataReceivedACK(int request_id) {
  DataReceivedACK(filter_->child_id(), request_id);
}

void ResourceDispatcherHostImpl::DataReceivedACK(int child_id,
                                                 int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(child_id, request_id));
  if (i == pending_requests_.end())
    return;

  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(i->second);

  // Decrement the number of pending data messages.
  info->DecrementPendingDataCount();

  // If the pending data count was higher than the max, resume the request.
  if (info->pending_data_count() == kMaxPendingDataMessages) {
    // Decrement the pending data count one more time because we also
    // incremented it before pausing the request.
    info->DecrementPendingDataCount();

    // Resume the request.
    PauseRequest(child_id, request_id, false);
  }
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
  int child_id = filter_->child_id();
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(child_id, request_id));
  if (i == pending_requests_.end())
    return;

  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(i->second);
  info->set_waiting_for_upload_progress_ack(false);
}

void ResourceDispatcherHostImpl::OnCancelRequest(int request_id) {
  CancelRequest(filter_->child_id(), request_id, true);
}

// Assigns the pending request a new routing_id because it was transferred
// to a new page.
void ResourceDispatcherHostImpl::OnTransferRequestToNewPage(int new_routing_id,
                                                            int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(filter_->child_id(), request_id));
  if (i == pending_requests_.end()) {
    // We probably want to remove this warning eventually, but I wanted to be
    // able to notice when this happens during initial development since it
    // should be rare and may indicate a bug.
    DVLOG(1) << "Updating a request that wasn't found";
    return;
  }
  net::URLRequest* request = i->second;
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  info->set_route_id(new_routing_id);
}

void ResourceDispatcherHostImpl::OnFollowRedirect(
    int request_id,
    bool has_new_first_party_for_cookies,
    const GURL& new_first_party_for_cookies) {
  FollowDeferredRedirect(filter_->child_id(), request_id,
                         has_new_first_party_for_cookies,
                         new_first_party_for_cookies);
}

ResourceRequestInfoImpl* ResourceDispatcherHostImpl::CreateRequestInfo(
    ResourceHandler* handler,
    int child_id,
    int route_id,
    bool download,
    ResourceContext* context) {
  return new ResourceRequestInfoImpl(
      handler,
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
      0,         // upload_size
      download,  // is_download
      download,  // allow_download
      false,     // has_user_gesture
      WebKit::WebReferrerPolicyDefault,
      context);
}

void ResourceDispatcherHostImpl::OnSwapOutACK(
    const ViewMsg_SwapOut_Params& params) {
  // Closes for cross-site transitions are handled such that the cross-site
  // transition continues.
  GlobalRequestID global_id(params.new_render_process_host_id,
                            params.new_request_id);
  PendingRequestList::iterator i = pending_requests_.find(global_id);
  if (i != pending_requests_.end()) {
    // The response we were meant to resume could have already been canceled.
    ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(i->second);
    if (info->cross_site_handler())
      info->cross_site_handler()->ResumeResponse();
  }
  // Update the RenderViewHost's internal state after the ACK.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OnSwapOutACKHelper,
                 params.closing_process_id,
                 params.closing_route_id));
}

void ResourceDispatcherHostImpl::OnDidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& security_info,
    const std::string& http_method,
    ResourceType::Type resource_type) {
  if (!url.is_valid() || !(url.SchemeIs("http") || url.SchemeIs("https")))
    return;

  filter_->GetURLRequestContext(resource_type)->http_transaction_factory()->
      GetCache()->OnExternalCacheHit(url, http_method);
}

// This function is only used for saving feature.
void ResourceDispatcherHostImpl::BeginSaveFile(
    const GURL& url,
    const content::Referrer& referrer,
    int child_id,
    int route_id,
    ResourceContext* context) {
  if (is_shutdown_)
    return;

  // http://crbug.com/90971
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);
  CHECK(!ContainsKey(canceled_resource_contexts_, context));

  scoped_refptr<ResourceHandler> handler(
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

  net::URLRequest* request = new net::URLRequest(url, this);
  request->set_method("GET");
  request->set_referrer(MaybeStripReferrer(referrer.url).spec());
  webkit_glue::ConfigureURLRequestForReferrerPolicy(request, referrer.policy);
  // So far, for saving page, we need fetch content from cache, in the
  // future, maybe we can use a configuration to configure this behavior.
  request->set_load_flags(net::LOAD_PREFERRING_CACHE);
  request->set_context(context->GetRequestContext());

  // Since we're just saving some resources we need, disallow downloading.
  ResourceRequestInfoImpl* extra_info =
      CreateRequestInfo(handler, child_id, route_id, false, context);
  extra_info->AssociateWithRequest(request);  // Request takes ownership.

  BeginRequestInternal(request);
}

void ResourceDispatcherHostImpl::FollowDeferredRedirect(
    int child_id,
    int request_id,
    bool has_new_first_party_for_cookies,
    const GURL& new_first_party_for_cookies) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(child_id, request_id));
  if (i == pending_requests_.end() || !i->second->status().is_success()) {
    DVLOG(1) << "FollowDeferredRedirect for invalid request";
    return;
  }

  if (has_new_first_party_for_cookies)
    i->second->set_first_party_for_cookies(new_first_party_for_cookies);
  i->second->FollowDeferredRedirect();
}

void ResourceDispatcherHostImpl::StartDeferredRequest(int child_id,
                                                      int request_id) {
  GlobalRequestID global_id(child_id, request_id);
  PendingRequestList::iterator i = pending_requests_.find(global_id);
  if (i == pending_requests_.end()) {
    // The request may have been destroyed
    LOG(WARNING) << "Trying to resume a non-existent request ("
                 << child_id << ", " << request_id << ")";
    return;
  }

  // TODO(eroman): are there other considerations for paused or blocked
  //               requests?

  StartRequest(i->second);
}

bool ResourceDispatcherHostImpl::WillSendData(int child_id,
                                              int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(child_id, request_id));
  if (i == pending_requests_.end()) {
    NOTREACHED() << "WillSendData for invalid request";
    return false;
  }

  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(i->second);

  info->IncrementPendingDataCount();
  if (info->pending_data_count() > kMaxPendingDataMessages) {
    // We reached the max number of data messages that can be sent to
    // the renderer for a given request. Pause the request and wait for
    // the renderer to start processing them before resuming it.
    PauseRequest(child_id, request_id, true);
    return false;
  }

  return true;
}

void ResourceDispatcherHostImpl::PauseRequest(int child_id,
                                              int request_id,
                                              bool pause) {
  GlobalRequestID global_id(child_id, request_id);
  PendingRequestList::iterator i = pending_requests_.find(global_id);
  if (i == pending_requests_.end()) {
    DVLOG(1) << "Pausing a request that wasn't found";
    return;
  }

  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(i->second);
  int pause_count = info->pause_count() + (pause ? 1 : -1);
  if (pause_count < 0) {
    NOTREACHED();  // Unbalanced call to pause.
    return;
  }
  info->set_pause_count(pause_count);

  VLOG(1) << "To pause (" << pause << "): " << i->second->url().spec();

  // If we're resuming, kick the request to start reading again. Run the read
  // asynchronously to avoid recursion problems.
  if (info->pause_count() == 0) {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(
            &ResourceDispatcherHostImpl::ResumeRequest,
            weak_factory_.GetWeakPtr(),
            global_id));
  }
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
  for (PendingRequestList::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->first.child_id == child_id) {
      ResourceRequestInfoImpl* info =
          ResourceRequestInfoImpl::ForRequest(i->second);
      GlobalRequestID id(child_id, i->first.request_id);
      DCHECK(id == i->first);
      // Don't cancel navigations that are transferring to another process,
      // since they belong to another process now.
      if (!info->is_download() &&
          (transferred_navigations_.find(id) ==
               transferred_navigations_.end()) &&
          (route_id == -1 || route_id == info->GetRouteID())) {
        matching_requests.push_back(
            GlobalRequestID(child_id, i->first.request_id));
      }
    }
  }

  // Remove matches.
  for (size_t i = 0; i < matching_requests.size(); ++i) {
    PendingRequestList::iterator iter =
        pending_requests_.find(matching_requests[i]);
    // Although every matching request was in pending_requests_ when we built
    // matching_requests, it is normal for a matching request to be not found
    // in pending_requests_ after we have removed some matching requests from
    // pending_requests_.  For example, deleting a net::URLRequest that has
    // exclusive (write) access to an HTTP cache entry may unblock another
    // net::URLRequest that needs exclusive access to the same cache entry, and
    // that net::URLRequest may complete and remove itself from
    // pending_requests_. So we need to check that iter is not equal to
    // pending_requests_.end().
    if (iter != pending_requests_.end())
      RemovePendingRequest(iter);
  }

  // Now deal with blocked requests if any.
  if (route_id != -1) {
    if (blocked_requests_map_.find(std::pair<int, int>(child_id, route_id)) !=
        blocked_requests_map_.end()) {
      CancelBlockedRequestsForRoute(child_id, route_id);
    }
  } else {
    // We have to do all render views for the process |child_id|.
    // Note that we have to do this in 2 passes as we cannot call
    // CancelBlockedRequestsForRoute while iterating over
    // blocked_requests_map_, as it modifies it.
    std::set<int> route_ids;
    for (BlockedRequestMap::const_iterator iter = blocked_requests_map_.begin();
         iter != blocked_requests_map_.end(); ++iter) {
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
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(child_id, request_id));
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Trying to remove a request that's not here";
    return;
  }
  RemovePendingRequest(i);
}

void ResourceDispatcherHostImpl::RemovePendingRequest(
    const PendingRequestList::iterator& iter) {
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(iter->second);

  // Remove the memory credit that we added when pushing the request onto
  // the pending list.
  IncrementOutstandingRequestsMemoryCost(-1 * info->memory_cost(),
                                         info->GetChildID());

  // Notify interested parties that the request object is going away.
  if (info->login_delegate())
    info->login_delegate()->OnRequestCancelled();
  if (info->ssl_client_auth_handler())
    info->ssl_client_auth_handler()->OnRequestCancelled();
  transferred_navigations_.erase(
      GlobalRequestID(info->GetChildID(), info->GetRequestID()));

  delete iter->second;
  pending_requests_.erase(iter);

  // If we have no more pending requests, then stop the load state monitor
  if (pending_requests_.empty() && update_load_states_timer_.get())
    update_load_states_timer_->Stop();
}

// net::URLRequest::Delegate ---------------------------------------------------

void ResourceDispatcherHostImpl::OnReceivedRedirect(net::URLRequest* request,
                                                    const GURL& new_url,
                                                    bool* defer_redirect) {
  VLOG(1) << "OnReceivedRedirect: " << request->url().spec();
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);

  DCHECK(request->status().is_success());

  if (info->process_type() != PROCESS_TYPE_PLUGIN &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->
          CanRequestURL(info->GetChildID(), new_url)) {
    VLOG(1) << "Denied unauthorized request for "
            << new_url.possibly_invalid_spec();

    // Tell the renderer that this request was disallowed.
    CancelRequestInternal(request, false);
    return;
  }

  NotifyReceivedRedirect(request, info->GetChildID(), new_url);

  if (HandleExternalProtocol(info->GetRequestID(), info->GetChildID(),
                             info->GetRouteID(), new_url,
                             info->GetResourceType(),
                             *request->context()->job_factory(),
                             info->resource_handler())) {
    // The request is complete so we can remove it.
    RemovePendingRequest(info->GetChildID(), info->GetRequestID());
    return;
  }

  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  PopulateResourceResponse(request, response);
  if (!info->resource_handler()->OnRequestRedirected(info->GetRequestID(),
                                                     new_url,
                                                     response, defer_redirect))
    CancelRequestInternal(request, false);
}

void ResourceDispatcherHostImpl::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  if (request->load_flags() & net::LOAD_DO_NOT_PROMPT_FOR_LOGIN) {
    request->CancelAuth();
    return;
  }

  if (delegate_ && !delegate_->AcceptAuthRequest(request, auth_info)) {
    request->CancelAuth();
    return;
  }

  // Prevent third-party content from prompting for login, unless it is
  // a proxy that is trying to authenticate.  This is often the foundation
  // of a scam to extract credentials for another domain from the user.
  if (!auth_info->is_proxy) {
    HttpAuthResourceType resource_type = HttpAuthResourceTypeOf(request);
    UMA_HISTOGRAM_ENUMERATION("Net.HttpAuthResource",
                              resource_type,
                              HTTP_AUTH_RESOURCE_LAST);

    if (resource_type == HTTP_AUTH_RESOURCE_BLOCKED_CROSS) {
      request->CancelAuth();
      return;
    }
  }


  // Create a login dialog on the UI thread to get authentication data,
  // or pull from cache and continue on the IO thread.
  // TODO(mpcomplete): We should block the parent tab while waiting for
  // authentication.
  // That would also solve the problem of the net::URLRequest being cancelled
  // before we receive authentication.
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  DCHECK(!info->login_delegate()) <<
      "OnAuthRequired called with login_delegate pending";
  if (delegate_) {
    info->set_login_delegate(delegate_->CreateLoginDelegate(
        auth_info, request));
  }
  if (!info->login_delegate())
    request->CancelAuth();
}

void ResourceDispatcherHostImpl::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK(request);
  if (delegate_ && !delegate_->AcceptSSLClientCertificateRequest(
          request, cert_request_info)) {
    request->Cancel();
    return;
  }

  if (cert_request_info->client_certs.empty()) {
    // No need to query the user if there are no certs to choose from.
    request->ContinueWithCertificate(NULL);
    return;
  }

  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  DCHECK(!info->ssl_client_auth_handler()) <<
      "OnCertificateRequested called with ssl_client_auth_handler pending";
  info->set_ssl_client_auth_handler(
      new SSLClientAuthHandler(request, cert_request_info));
  info->ssl_client_auth_handler()->SelectCertificate();
}

void ResourceDispatcherHostImpl::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool is_hsts_host) {
  DCHECK(request);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  DCHECK(info);
  GlobalRequestID request_id(info->GetChildID(), info->GetRequestID());
  int render_process_id;
  int render_view_id;
  if(!info->GetAssociatedRenderView(&render_process_id, &render_view_id))
    NOTREACHED();
  SSLManager::OnSSLCertificateError(this, request_id, info->GetResourceType(),
      request->url(), render_process_id, render_view_id, ssl_info,
      is_hsts_host);
}

void ResourceDispatcherHostImpl::OnResponseStarted(net::URLRequest* request) {
  VLOG(1) << "OnResponseStarted: " << request->url().spec();
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);

  if (request->status().is_success()) {
    if (PauseRequestIfNeeded(info)) {
      VLOG(1) << "OnResponseStarted pausing: " << request->url().spec();
      return;
    }

    // We want to send a final upload progress message prior to sending
    // the response complete message even if we're waiting for an ack to
    // to a previous upload progress message.
    info->set_waiting_for_upload_progress_ack(false);
    MaybeUpdateUploadProgress(info, request);

    if (!CompleteResponseStarted(request)) {
      CancelRequestInternal(request, false);
    } else {
      // Check if the handler paused the request in their OnResponseStarted.
      if (PauseRequestIfNeeded(info)) {
        VLOG(1) << "OnResponseStarted pausing2: " << request->url().spec();
        return;
      }

      StartReading(request);
    }
  } else {
    ResponseCompleted(request);
  }
}

bool ResourceDispatcherHostImpl::CompleteResponseStarted(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);

  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  PopulateResourceResponse(request, response);

  if (request->ssl_info().cert) {
    int cert_id =
        CertStore::GetInstance()->StoreCert(request->ssl_info().cert,
                                            info->GetChildID());
    response->security_info = SerializeSecurityInfo(
        cert_id, request->ssl_info().cert_status,
        request->ssl_info().security_bits,
        request->ssl_info().connection_status);
  } else {
    // We should not have any SSL state.
    DCHECK(!request->ssl_info().cert_status &&
           request->ssl_info().security_bits == -1 &&
           !request->ssl_info().connection_status);
  }

  NotifyResponseStarted(request, info->GetChildID());
  info->set_called_on_response_started(true);
  return info->resource_handler()->OnResponseStarted(info->GetRequestID(),
                                                     response.get());
}

void ResourceDispatcherHostImpl::CancelRequest(int child_id,
                                               int request_id,
                                               bool from_renderer) {
  GlobalRequestID id(child_id, request_id);
  if (from_renderer) {
    // When the old renderer dies, it sends a message to us to cancel its
    // requests.
    if (transferred_navigations_.find(id) != transferred_navigations_.end())
      return;
  }

  PendingRequestList::iterator i = pending_requests_.find(id);
  if (i == pending_requests_.end()) {
    // We probably want to remove this warning eventually, but I wanted to be
    // able to notice when this happens during initial development since it
    // should be rare and may indicate a bug.
    DVLOG(1) << "Canceling a request that wasn't found";
    return;
  }
  net::URLRequest* request = i->second;

  bool started_before_cancel = request->is_pending();
  if (CancelRequestInternal(request, from_renderer) &&
      !started_before_cancel) {
    // If the request isn't in flight, then we won't get an asynchronous
    // notification from the request, so we have to signal ourselves to finish
    // this request.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::CallResponseCompleted,
                   base::Unretained(this),
                   child_id,
                   request_id));
  }
}

bool ResourceDispatcherHostImpl::CancelRequestInternal(net::URLRequest* request,
                                                       bool from_renderer) {
  VLOG(1) << "CancelRequest: " << request->url().spec();

  // WebKit will send us a cancel for downloads since it no longer handles them.
  // In this case, ignore the cancel since we handle downloads in the browser.
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  if (!from_renderer || !info->is_download()) {
    if (info->login_delegate()) {
      info->login_delegate()->OnRequestCancelled();
      info->set_login_delegate(NULL);
    }
    if (info->ssl_client_auth_handler()) {
      info->ssl_client_auth_handler()->OnRequestCancelled();
      info->set_ssl_client_auth_handler(NULL);
    }
    request->Cancel();
    // Our callers assume |request| is valid after we return.
    DCHECK(IsValidRequest(request));
    return true;
  }

  // Do not remove from the pending requests, as the request will still
  // call AllDataReceived, and may even have more data before it does
  // that.
  return false;
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
    net::URLRequest* request) {
  DCHECK(!request->is_pending());
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);

  if ((TimeTicks::Now() - last_user_gesture_time_) <
      TimeDelta::FromMilliseconds(kUserGestureWindowMs)) {
    request->set_load_flags(
        request->load_flags() | net::LOAD_MAYBE_USER_GESTURE);
  }

  // Add the memory estimate that starting this request will consume.
  info->set_memory_cost(CalculateApproximateMemoryCost(request));
  int memory_cost = IncrementOutstandingRequestsMemoryCost(info->memory_cost(),
                                                           info->GetChildID());

  // If enqueing/starting this request will exceed our per-process memory
  // bound, abort it right away.
  if (memory_cost > max_outstanding_requests_cost_per_process_) {
    // We call "CancelWithError()" as a way of setting the net::URLRequest's
    // status -- it has no effect beyond this, since the request hasn't started.
    request->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);

    // TODO(eroman): this is kinda funky -- we insert the unstarted request into
    // |pending_requests_| simply to please ResponseCompleted().
    GlobalRequestID global_id(info->GetChildID(), info->GetRequestID());
    pending_requests_[global_id] = request;
    ResponseCompleted(request);
    return;
  }

  std::pair<int, int> pair_id(info->GetChildID(), info->GetRouteID());
  BlockedRequestMap::const_iterator iter = blocked_requests_map_.find(pair_id);
  if (iter != blocked_requests_map_.end()) {
    // The request should be blocked.
    iter->second->push_back(request);
    return;
  }

  GlobalRequestID global_id(info->GetChildID(), info->GetRequestID());
  pending_requests_[global_id] = request;

  // Give the resource handlers an opportunity to delay the net::URLRequest from
  // being started.
  //
  // There are three cases:
  //
  //   (1) if OnWillStart() returns false, the request is cancelled (regardless
  //       of whether |defer_start| was set).
  //   (2) If |defer_start| was set to true, then the request is not added
  //       into the resource queue, and will only be started in response to
  //       calling StartDeferredRequest().
  //   (3) If |defer_start| is not set, then the request is inserted into
  //       the resource_queue_ (which may pause it further, or start it).
  bool defer_start = false;
  if (!info->resource_handler()->OnWillStart(
          info->GetRequestID(), request->url(),
          &defer_start)) {
    CancelRequestInternal(request, false);
    return;
  }

  if (!defer_start)
    StartRequest(request);
}

void ResourceDispatcherHostImpl::StartRequest(net::URLRequest* request) {
  request->Start();

  // Make sure we have the load state monitor running
  if (!update_load_states_timer_->IsRunning()) {
    update_load_states_timer_->Start(FROM_HERE,
        TimeDelta::FromMilliseconds(kUpdateLoadStatesIntervalMsec),
        this, &ResourceDispatcherHostImpl::UpdateLoadStates);
  }
}

bool ResourceDispatcherHostImpl::PauseRequestIfNeeded(
    ResourceRequestInfoImpl* info) {
  if (info->pause_count() > 0)
    info->set_is_paused(true);
  return info->is_paused();
}

void ResourceDispatcherHostImpl::ResumeRequest(
    const GlobalRequestID& request_id) {
  PendingRequestList::iterator i = pending_requests_.find(request_id);
  if (i == pending_requests_.end())  // The request may have been destroyed
    return;

  net::URLRequest* request = i->second;
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);

  // We may already be unpaused, or the pause count may have increased since we
  // posted the task to call ResumeRequest.
  if (!info->is_paused())
    return;
  info->set_is_paused(false);
  if (PauseRequestIfNeeded(info))
    return;

  VLOG(1) << "Resuming: \"" << i->second->url().spec() << "\""
          << " paused_read_bytes = " << info->paused_read_bytes()
          << " called response started = " << info->called_on_response_started()
          << " started reading = " << info->has_started_reading();

  if (info->called_on_response_started()) {
    if (info->has_started_reading()) {
      OnReadCompleted(i->second, info->paused_read_bytes());
    } else {
      StartReading(request);
    }
  } else {
    OnResponseStarted(i->second);
  }
}

void ResourceDispatcherHostImpl::StartReading(net::URLRequest* request) {
  // Start reading.
  int bytes_read = 0;
  if (Read(request, &bytes_read)) {
    OnReadCompleted(request, bytes_read);
  } else if (!request->status().is_io_pending()) {
    DCHECK(!ResourceRequestInfoImpl::ForRequest(request)->is_paused());
    // If the error is not an IO pending, then we're done reading.
    ResponseCompleted(request);
  }
}

bool ResourceDispatcherHostImpl::Read(net::URLRequest* request,
                                      int* bytes_read) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  DCHECK(!info->is_paused());

  net::IOBuffer* buf;
  int buf_size;
  if (!info->resource_handler()->OnWillRead(info->GetRequestID(),
                                            &buf, &buf_size, -1)) {
    return false;
  }

  DCHECK(buf);
  DCHECK(buf_size > 0);

  info->set_has_started_reading(true);
  return request->Read(buf, buf_size, bytes_read);
}

void ResourceDispatcherHostImpl::OnReadCompleted(net::URLRequest* request,
                                                 int bytes_read) {
  DCHECK(request);
  VLOG(1) << "OnReadCompleted: \"" << request->url().spec() << "\""
          << " bytes_read = " << bytes_read;
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);

  // bytes_read == -1 always implies an error, so we want to skip the pause
  // checks and just call ResponseCompleted.
  if (bytes_read == -1) {
    DCHECK(!request->status().is_success());

    ResponseCompleted(request);
    return;
  }

  // OnReadCompleted can be called without Read (e.g., for chrome:// URLs).
  // Make sure we know that a read has begun.
  info->set_has_started_reading(true);

  if (PauseRequestIfNeeded(info)) {
    info->set_paused_read_bytes(bytes_read);
    VLOG(1) << "OnReadCompleted pausing: \"" << request->url().spec() << "\""
            << " bytes_read = " << bytes_read;
    return;
  }

  if (request->status().is_success() && CompleteRead(request, &bytes_read)) {
    // The request can be paused if we realize that the renderer is not
    // servicing messages fast enough.
    if (info->pause_count() == 0 &&
        Read(request, &bytes_read) &&
        request->status().is_success()) {
      if (bytes_read == 0) {
        CompleteRead(request, &bytes_read);
      } else {
        // Force the next CompleteRead / Read pair to run as a separate task.
        // This avoids a fast, large network request from monopolizing the IO
        // thread and starving other IO operations from running.
        VLOG(1) << "OnReadCompleted postponing: \""
                << request->url().spec() << "\""
                << " bytes_read = " << bytes_read;
        info->set_paused_read_bytes(bytes_read);
        info->set_is_paused(true);
        GlobalRequestID id(info->GetChildID(), info->GetRequestID());
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(
                &ResourceDispatcherHostImpl::ResumeRequest,
                weak_factory_.GetWeakPtr(), id));
        return;
      }
    }
  }

  if (PauseRequestIfNeeded(info)) {
    info->set_paused_read_bytes(bytes_read);
    VLOG(1) << "OnReadCompleted (CompleteRead) pausing: \""
            << request->url().spec() << "\""
            << " bytes_read = " << bytes_read;
    return;
  }

  // If the status is not IO pending then we've either finished (success) or we
  // had an error.  Either way, we're done!
  if (!request->status().is_io_pending())
    ResponseCompleted(request);
}

bool ResourceDispatcherHostImpl::CompleteRead(net::URLRequest* request,
                                              int* bytes_read) {
  if (!request || !request->status().is_success()) {
    NOTREACHED();
    return false;
  }

  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  if (!info->resource_handler()->OnReadCompleted(info->GetRequestID(),
                                                 bytes_read)) {
    CancelRequestInternal(request, false);
    return false;
  }

  return *bytes_read != 0;
}

void ResourceDispatcherHostImpl::ResponseCompleted(net::URLRequest* request) {
  VLOG(1) << "ResponseCompleted: " << request->url().spec();
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);

  // If the load for a main frame has failed, track it in a histogram,
  // since it will probably cause the user to see an error page.
  if (!request->status().is_success() &&
      request->status().error() != net::ERR_ABORTED) {
    if (info->GetResourceType() == ResourceType::MAIN_FRAME) {
      // This enumeration has "2" appended to its name to distinguish it from
      // its original version. We changed the buckets at one point (added
      // guard buckets by using CustomHistogram::ArrayToCustomRanges).
      UMA_HISTOGRAM_CUSTOM_ENUMERATION(
          "Net.ErrorCodesForMainFrame2",
          -request->status().error(),
          base::CustomHistogram::ArrayToCustomRanges(
              kAllNetErrorCodes, arraysize(kAllNetErrorCodes)));

      if (request->url().SchemeIsSecure() &&
          request->url().host() == "www.google.com") {
        UMA_HISTOGRAM_CUSTOM_ENUMERATION(
            "Net.ErrorCodesForHTTPSGoogleMainFrame",
            -request->status().error(),
            base::CustomHistogram::ArrayToCustomRanges(
                kAllNetErrorCodes, arraysize(kAllNetErrorCodes)));
      }
    } else {
      UMA_HISTOGRAM_CUSTOM_ENUMERATION(
          "Net.ErrorCodesForSubresources",
          -request->status().error(),
          base::CustomHistogram::ArrayToCustomRanges(
              kAllNetErrorCodes, arraysize(kAllNetErrorCodes)));
    }
  }

  std::string security_info;
  const net::SSLInfo& ssl_info = request->ssl_info();
  if (ssl_info.cert != NULL) {
    int cert_id = CertStore::GetInstance()->StoreCert(ssl_info.cert,
                                                      info->GetChildID());
    security_info = SerializeSecurityInfo(
        cert_id, ssl_info.cert_status, ssl_info.security_bits,
        ssl_info.connection_status);
  }

  if (info->resource_handler()->OnResponseCompleted(info->GetRequestID(),
                                                    request->status(),
                                                    security_info)) {

    // The request is complete so we can remove it.
    RemovePendingRequest(info->GetChildID(), info->GetRequestID());
  }
  // If the handler's OnResponseCompleted returns false, we are deferring the
  // call until later.  We will notify the world and clean up when we resume.
}

void ResourceDispatcherHostImpl::CallResponseCompleted(int child_id,
                                                       int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(child_id, request_id));
  if (i != pending_requests_.end())
    ResponseCompleted(i->second);
}

// SSLErrorHandler::Delegate ---------------------------------------------------

void ResourceDispatcherHostImpl::CancelSSLRequest(
    const GlobalRequestID& id,
    int error,
    const net::SSLInfo* ssl_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequest* request = GetURLRequest(id);
  // The request can be NULL if it was cancelled by the renderer (as the
  // request of the user navigating to a new page from the location bar).
  if (!request || !request->is_pending())
    return;
  DVLOG(1) << "CancelSSLRequest() url: " << request->url().spec();
  if (ssl_info)
    request->CancelWithSSLError(error, *ssl_info);
  else
    request->CancelWithError(error);
}

void ResourceDispatcherHostImpl::ContinueSSLRequest(
    const GlobalRequestID& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequest* request = GetURLRequest(id);
  // The request can be NULL if it was cancelled by the renderer (as the
  // request of the user navigating to a new page from the location bar).
  if (!request)
    return;
  DVLOG(1) << "ContinueSSLRequest() url: " << request->url().spec();
  request->ContinueDespiteLastError();
}

void ResourceDispatcherHostImpl::OnUserGesture(WebContentsImpl* contents) {
  last_user_gesture_time_ = TimeTicks::Now();
}

net::URLRequest* ResourceDispatcherHostImpl::GetURLRequest(
    const GlobalRequestID& request_id) const {
  // This should be running in the IO loop.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  PendingRequestList::const_iterator i = pending_requests_.find(request_id);
  if (i == pending_requests_.end())
    return NULL;

  return i->second;
}

static int GetCertID(net::URLRequest* request, int child_id) {
  if (request->ssl_info().cert) {
    return CertStore::GetInstance()->StoreCert(request->ssl_info().cert,
                                               child_id);
  }
  return 0;
}

void ResourceDispatcherHostImpl::NotifyResponseStarted(net::URLRequest* request,
                                                       int child_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  int render_process_id, render_view_id;
  if (!RenderViewForRequest(request, &render_process_id, &render_view_id))
    return;

  // Notify the observers on the UI thread.
  ResourceRequestDetails* detail = new ResourceRequestDetails(
      request, GetCertID(request, child_id));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &ResourceDispatcherHostImpl::NotifyOnUI<ResourceRequestDetails>,
          static_cast<int>(NOTIFICATION_RESOURCE_RESPONSE_STARTED),
          render_process_id, render_view_id, detail));
}

void ResourceDispatcherHostImpl::NotifyReceivedRedirect(
    net::URLRequest* request,
    int child_id,
    const GURL& new_url) {
  int render_process_id, render_view_id;
  if (!RenderViewForRequest(request, &render_process_id, &render_view_id))
    return;

  // Notify the observers on the UI thread.
  ResourceRedirectDetails* detail = new ResourceRedirectDetails(
      request, GetCertID(request, child_id), new_url);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &ResourceDispatcherHostImpl::NotifyOnUI<ResourceRedirectDetails>,
          static_cast<int>(NOTIFICATION_RESOURCE_RECEIVED_REDIRECT),
          render_process_id, render_view_id, detail));
}

template <class T>
void ResourceDispatcherHostImpl::NotifyOnUI(int type,
                                            int render_process_id,
                                            int render_view_id,
                                            T* detail) {
  RenderViewHostImpl* rvh =
      RenderViewHostImpl::FromID(render_process_id, render_view_id);
  if (rvh) {
    RenderViewHostDelegate* rvhd = rvh->GetDelegate();
    NotificationService::current()->Notify(
        type, Source<WebContents>(rvhd->GetAsWebContents()),
        Details<T>(detail));
  }
  delete detail;
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

  PendingRequestList::const_iterator i;

  // Determine the largest upload size of all requests
  // in each View (good chance it's zero).
  std::map<std::pair<int, int>, uint64> largest_upload_size;
  for (i = pending_requests_.begin(); i != pending_requests_.end(); ++i) {
    net::URLRequest* request = i->second;
    ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(request);
    uint64 upload_size = info->GetUploadSize();
    if (request->GetLoadState().state != net::LOAD_STATE_SENDING_REQUEST)
      upload_size = 0;
    std::pair<int, int> key(info->GetChildID(), info->GetRouteID());
    if (upload_size && largest_upload_size[key] < upload_size)
      largest_upload_size[key] = upload_size;
  }

  for (i = pending_requests_.begin(); i != pending_requests_.end(); ++i) {
    net::URLRequest* request = i->second;
    net::LoadStateWithParam load_state = request->GetLoadState();
    ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(request);
    std::pair<int, int> key(info->GetChildID(), info->GetRouteID());

    // We also poll for upload progress on this timer and send upload
    // progress ipc messages to the plugin process.
    MaybeUpdateUploadProgress(info, request);

    // If a request is uploading data, ignore all other requests so that the
    // upload progress takes priority for being shown in the status bar.
    if (largest_upload_size.find(key) != largest_upload_size.end() &&
        info->GetUploadSize() < largest_upload_size[key])
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
    load_info.upload_size = info->GetUploadSize();
    load_info.upload_position = request->GetUploadProgress();
  }

  if (info_map.empty())
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&LoadInfoUpdateCallback, info_map));
}

// Calls the ResourceHandler to send upload progress messages to the renderer.
void ResourceDispatcherHostImpl::MaybeUpdateUploadProgress(
    ResourceRequestInfoImpl *info,
    net::URLRequest *request) {

  if (!info->GetUploadSize() || info->waiting_for_upload_progress_ack())
    return;

  uint64 size = info->GetUploadSize();
  uint64 position = request->GetUploadProgress();
  if (position == info->last_upload_position())
    return;  // no progress made since last time

  const uint64 kHalfPercentIncrements = 200;
  const TimeDelta kOneSecond = TimeDelta::FromMilliseconds(1000);

  uint64 amt_since_last = position - info->last_upload_position();
  TimeDelta time_since_last = TimeTicks::Now() - info->last_upload_ticks();

  bool is_finished = (size == position);
  bool enough_new_progress = (amt_since_last > (size / kHalfPercentIncrements));
  bool too_much_time_passed = time_since_last > kOneSecond;

  if (is_finished || enough_new_progress || too_much_time_passed) {
    if (request->load_flags() & net::LOAD_ENABLE_UPLOAD_PROGRESS) {
      info->resource_handler()->OnUploadProgress(info->GetRequestID(),
                                                 position, size);
      info->set_waiting_for_upload_progress_ack(true);
    }
    info->set_last_upload_ticks(TimeTicks::Now());
    info->set_last_upload_position(position);
  }
}

void ResourceDispatcherHostImpl::BlockRequestsForRoute(int child_id,
                                                       int route_id) {
  std::pair<int, int> key(child_id, route_id);
  DCHECK(blocked_requests_map_.find(key) == blocked_requests_map_.end()) <<
      "BlockRequestsForRoute called  multiple time for the same RVH";
  blocked_requests_map_[key] = new BlockedRequestsList();
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
  BlockedRequestMap::iterator iter = blocked_requests_map_.find(
      std::pair<int, int>(child_id, route_id));
  if (iter == blocked_requests_map_.end()) {
    // It's possible to reach here if the renderer crashed while an interstitial
    // page was showing.
    return;
  }

  BlockedRequestsList* requests = iter->second;

  // Removing the vector from the map unblocks any subsequent requests.
  blocked_requests_map_.erase(iter);

  for (BlockedRequestsList::iterator req_iter = requests->begin();
       req_iter != requests->end(); ++req_iter) {
    // Remove the memory credit that we added when pushing the request onto
    // the blocked list.
    net::URLRequest* request = *req_iter;
    ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(request);
    IncrementOutstandingRequestsMemoryCost(-1 * info->memory_cost(),
                                           info->GetChildID());
    if (cancel_requests)
      delete request;
    else
      BeginRequestInternal(request);
  }

  delete requests;
}

bool ResourceDispatcherHostImpl::IsValidRequest(net::URLRequest* request) {
  if (!request)
    return false;
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  return pending_requests_.find(
      GlobalRequestID(info->GetChildID(), info->GetRequestID())) !=
      pending_requests_.end();
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
    const GlobalRequestID& transferred_request_id) const {
  return transferred_navigations_.find(transferred_request_id) !=
      transferred_navigations_.end();
}

}  // namespace content
