// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/appcache/web_application_cache_host_impl.h"

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebFrame.h"

using blink::WebApplicationCacheHost;
using blink::WebApplicationCacheHostClient;
using blink::WebDataSource;
using blink::WebFrame;
using blink::WebURLRequest;
using blink::WebURL;
using blink::WebURLResponse;
using blink::WebVector;
using appcache::AppCacheBackend;
using appcache::AppCacheResourceInfo;

namespace content {

namespace {

// Note: the order of the elements in this array must match those
// of the EventID enum in appcache_interfaces.h.
const char* kEventNames[] = {
  "Checking", "Error", "NoUpdate", "Downloading", "Progress",
  "UpdateReady", "Cached", "Obsolete"
};

typedef IDMap<WebApplicationCacheHostImpl> HostsMap;

HostsMap* all_hosts() {
  static HostsMap* map = new HostsMap;
  return map;
}

GURL ClearUrlRef(const GURL& url) {
  if (!url.has_ref())
    return url;
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

}  // anon namespace

WebApplicationCacheHostImpl* WebApplicationCacheHostImpl::FromId(int id) {
  return all_hosts()->Lookup(id);
}

WebApplicationCacheHostImpl* WebApplicationCacheHostImpl::FromFrame(
    const WebFrame* frame) {
  if (!frame)
    return NULL;
  WebDataSource* data_source = frame->dataSource();
  if (!data_source)
    return NULL;
  return static_cast<WebApplicationCacheHostImpl*>
      (data_source->applicationCacheHost());
}

WebApplicationCacheHostImpl::WebApplicationCacheHostImpl(
    WebApplicationCacheHostClient* client,
    AppCacheBackend* backend)
    : client_(client),
      backend_(backend),
      host_id_(all_hosts()->Add(this)),
      status_(appcache::UNCACHED),
      is_scheme_supported_(false),
      is_get_method_(false),
      is_new_master_entry_(MAYBE),
      was_select_cache_called_(false) {
  DCHECK(client && backend && (host_id_ != appcache::kNoHostId));

  backend_->RegisterHost(host_id_);
}

WebApplicationCacheHostImpl::~WebApplicationCacheHostImpl() {
  backend_->UnregisterHost(host_id_);
  all_hosts()->Remove(host_id_);
}

void WebApplicationCacheHostImpl::OnCacheSelected(
    const appcache::AppCacheInfo& info) {
  cache_info_ = info;
  client_->didChangeCacheAssociation();
}

void WebApplicationCacheHostImpl::OnStatusChanged(appcache::Status status) {
  // TODO(michaeln): delete me, not used
}

void WebApplicationCacheHostImpl::OnEventRaised(appcache::EventID event_id) {
  DCHECK(event_id != appcache::PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK(event_id != appcache::ERROR_EVENT);  // See OnErrorEventRaised.

  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char* kFormatString = "Application Cache %s event";
  std::string message = base::StringPrintf(kFormatString,
                                           kEventNames[event_id]);
  OnLogMessage(appcache::LOG_INFO, message);

  switch (event_id) {
    case appcache::CHECKING_EVENT:
      status_ = appcache::CHECKING;
      break;
    case appcache::DOWNLOADING_EVENT:
      status_ = appcache::DOWNLOADING;
      break;
    case appcache::UPDATE_READY_EVENT:
      status_ = appcache::UPDATE_READY;
      break;
    case appcache::CACHED_EVENT:
    case appcache::NO_UPDATE_EVENT:
      status_ = appcache::IDLE;
      break;
    case appcache::OBSOLETE_EVENT:
      status_ = appcache::OBSOLETE;
      break;
    default:
      NOTREACHED();
      break;
  }

  client_->notifyEventListener(static_cast<EventID>(event_id));
}

void WebApplicationCacheHostImpl::OnProgressEventRaised(
    const GURL& url, int num_total, int num_complete) {
  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char* kFormatString = "Application Cache Progress event (%d of %d) %s";
  std::string message = base::StringPrintf(kFormatString, num_complete,
                                           num_total, url.spec().c_str());
  OnLogMessage(appcache::LOG_INFO, message);
  status_ = appcache::DOWNLOADING;
  client_->notifyProgressEventListener(url, num_total, num_complete);
}

void WebApplicationCacheHostImpl::OnErrorEventRaised(
    const std::string& message) {
  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char* kFormatString = "Application Cache Error event: %s";
  std::string full_message = base::StringPrintf(kFormatString,
                                                message.c_str());
  OnLogMessage(appcache::LOG_ERROR, full_message);

  status_ = cache_info_.is_complete ? appcache::IDLE : appcache::UNCACHED;
  client_->notifyEventListener(static_cast<EventID>(appcache::ERROR_EVENT));
}

void WebApplicationCacheHostImpl::willStartMainResourceRequest(
    WebURLRequest& request, const WebFrame* frame) {
  WebApplicationCacheHostImpl* spawning_host = NULL;
  if (frame) {
    const WebFrame* spawning_frame = frame->parent();
    if (!spawning_frame)
      spawning_frame = frame->opener();
    if (!spawning_frame)
      spawning_frame = frame;

    spawning_host = FromFrame(spawning_frame);
  }
  willStartMainResourceRequest(request, spawning_host);
}

void WebApplicationCacheHostImpl::willStartMainResourceRequest(
    WebURLRequest& request, const WebApplicationCacheHost* spawning_host) {
  request.setAppCacheHostID(host_id_);

  original_main_resource_url_ = ClearUrlRef(request.url());

  std::string method = request.httpMethod().utf8();
  is_get_method_ = (method == appcache::kHttpGETMethod);
  DCHECK(method == StringToUpperASCII(method));

  const WebApplicationCacheHostImpl* spawning_host_impl =
      static_cast<const WebApplicationCacheHostImpl*>(spawning_host);
  if (spawning_host_impl && (spawning_host_impl != this) &&
      (spawning_host_impl->status_ != appcache::UNCACHED)) {
    backend_->SetSpawningHostId(host_id_, spawning_host_impl->host_id());
  }
}

void WebApplicationCacheHostImpl::willStartSubResourceRequest(
    WebURLRequest& request) {
  request.setAppCacheHostID(host_id_);
}

void WebApplicationCacheHostImpl::selectCacheWithoutManifest() {
  if (was_select_cache_called_)
    return;
  was_select_cache_called_ = true;

  status_ = (document_response_.appCacheID() == appcache::kNoCacheId) ?
      appcache::UNCACHED : appcache::CHECKING;
  is_new_master_entry_ = NO;
  backend_->SelectCache(host_id_, document_url_,
                        document_response_.appCacheID(),
                        GURL());
}

bool WebApplicationCacheHostImpl::selectCacheWithManifest(
    const WebURL& manifest_url) {
  if (was_select_cache_called_)
    return true;
  was_select_cache_called_ = true;

  GURL manifest_gurl(ClearUrlRef(manifest_url));

  // 6.9.6 The application cache selection algorithm
  // Check for new 'master' entries.
  if (document_response_.appCacheID() == appcache::kNoCacheId) {
    if (is_scheme_supported_ && is_get_method_ &&
        (manifest_gurl.GetOrigin() == document_url_.GetOrigin())) {
      status_ = appcache::CHECKING;
      is_new_master_entry_ = YES;
    } else {
      status_ = appcache::UNCACHED;
      is_new_master_entry_ = NO;
      manifest_gurl = GURL();
    }
    backend_->SelectCache(
        host_id_, document_url_, appcache::kNoCacheId, manifest_gurl);
    return true;
  }

  DCHECK_EQ(NO, is_new_master_entry_);

  // 6.9.6 The application cache selection algorithm
  // Check for 'foreign' entries.
  GURL document_manifest_gurl(document_response_.appCacheManifestURL());
  if (document_manifest_gurl != manifest_gurl) {
    backend_->MarkAsForeignEntry(host_id_, document_url_,
                                 document_response_.appCacheID());
    status_ = appcache::UNCACHED;
    return false;  // the navigation will be restarted
  }

  status_ = appcache::CHECKING;

  // Its a 'master' entry thats already in the cache.
  backend_->SelectCache(host_id_, document_url_,
                        document_response_.appCacheID(),
                        manifest_gurl);
  return true;
}

void WebApplicationCacheHostImpl::didReceiveResponseForMainResource(
    const WebURLResponse& response) {
  document_response_ = response;
  document_url_ = ClearUrlRef(document_response_.url());
  if (document_url_ != original_main_resource_url_)
    is_get_method_ = true;  // A redirect was involved.
  original_main_resource_url_ = GURL();

  is_scheme_supported_ =  appcache::IsSchemeSupported(document_url_);
  if ((document_response_.appCacheID() != appcache::kNoCacheId) ||
      !is_scheme_supported_ || !is_get_method_)
    is_new_master_entry_ = NO;
}

void WebApplicationCacheHostImpl::didReceiveDataForMainResource(
    const char* data, int len) {
  if (is_new_master_entry_ == NO)
    return;
  // TODO(michaeln): write me
}

void WebApplicationCacheHostImpl::didFinishLoadingMainResource(bool success) {
  if (is_new_master_entry_ == NO)
    return;
  // TODO(michaeln): write me
}

WebApplicationCacheHost::Status WebApplicationCacheHostImpl::status() {
  return static_cast<WebApplicationCacheHost::Status>(status_);
}

bool WebApplicationCacheHostImpl::startUpdate() {
  if (!backend_->StartUpdate(host_id_))
    return false;
  if (status_ == appcache::IDLE || status_ == appcache::UPDATE_READY)
    status_ = appcache::CHECKING;
  else
    status_ = backend_->GetStatus(host_id_);
  return true;
}

bool WebApplicationCacheHostImpl::swapCache() {
  if (!backend_->SwapCache(host_id_))
    return false;
  status_ = backend_->GetStatus(host_id_);
  return true;
}

void WebApplicationCacheHostImpl::getAssociatedCacheInfo(
    WebApplicationCacheHost::CacheInfo* info) {
  info->manifestURL = cache_info_.manifest_url;
  if (!cache_info_.is_complete)
    return;
  info->creationTime = cache_info_.creation_time.ToDoubleT();
  info->updateTime = cache_info_.last_update_time.ToDoubleT();
  info->totalSize = cache_info_.size;
}

void WebApplicationCacheHostImpl::getResourceList(
    WebVector<ResourceInfo>* resources) {
  if (!cache_info_.is_complete)
    return;
  std::vector<AppCacheResourceInfo> resource_infos;
  backend_->GetResourceList(host_id_, &resource_infos);

  WebVector<ResourceInfo> web_resources(resource_infos.size());
  for (size_t i = 0; i < resource_infos.size(); ++i) {
    web_resources[i].size = resource_infos[i].size;
    web_resources[i].isMaster = resource_infos[i].is_master;
    web_resources[i].isExplicit = resource_infos[i].is_explicit;
    web_resources[i].isManifest = resource_infos[i].is_manifest;
    web_resources[i].isForeign = resource_infos[i].is_foreign;
    web_resources[i].isFallback = resource_infos[i].is_fallback;
    web_resources[i].url = resource_infos[i].url;
  }
  resources->swap(web_resources);
}

}  // namespace content
