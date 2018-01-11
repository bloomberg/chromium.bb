// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RESOURCE_MESSAGES_H_
#define CONTENT_COMMON_RESOURCE_MESSAGES_H_

// IPC messages for resource loading.
//
// WE ARE DEPRECATING THIS FILE. DO NOT ADD A NEW MESSAGE.
// NOTE: All messages must send an |int request_id| as their first parameter.

#include <stdint.h>

#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "content/common/content_param_traits_macros.h"
#include "content/common/navigation_params.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_request_body.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/service_worker_modes.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/network_param_ipc_traits.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "third_party/WebKit/public/platform/WebMixedContentContextType.h"

#ifndef INTERNAL_CONTENT_COMMON_RESOURCE_MESSAGES_H_
#define INTERNAL_CONTENT_COMMON_RESOURCE_MESSAGES_H_

namespace net {
struct LoadTimingInfo;
}

namespace IPC {

template <>
struct CONTENT_EXPORT ParamTraits<network::DataElement> {
  typedef network::DataElement param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<net::LoadTimingInfo> {
  typedef net::LoadTimingInfo param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<scoped_refptr<content::ResourceRequestBody>> {
  typedef scoped_refptr<content::ResourceRequestBody> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // INTERNAL_CONTENT_COMMON_RESOURCE_MESSAGES_H_

#define IPC_MESSAGE_START ResourceMsgStart
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE( \
    net::HttpResponseInfo::ConnectionInfo, \
    net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS - 1)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::FetchRequestMode,
                          network::mojom::FetchRequestMode::kLast)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::FetchCredentialsMode,
                          network::mojom::FetchCredentialsMode::kLast)

IPC_ENUM_TRAITS_MAX_VALUE(content::FetchRedirectMode,
                          content::FetchRedirectMode::LAST)

IPC_ENUM_TRAITS_MAX_VALUE(content::ServiceWorkerMode,
                          content::ServiceWorkerMode::LAST)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebMixedContentContextType,
                          blink::WebMixedContentContextType::kLast)

IPC_STRUCT_TRAITS_BEGIN(content::ResourceResponseHead)
IPC_STRUCT_TRAITS_PARENT(content::ResourceResponseInfo)
  IPC_STRUCT_TRAITS_MEMBER(request_start)
  IPC_STRUCT_TRAITS_MEMBER(response_start)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyncLoadResult)
  IPC_STRUCT_TRAITS_PARENT(content::ResourceResponseHead)
  IPC_STRUCT_TRAITS_MEMBER(error_code)
  IPC_STRUCT_TRAITS_MEMBER(final_url)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ResourceResponseInfo)
  IPC_STRUCT_TRAITS_MEMBER(request_time)
  IPC_STRUCT_TRAITS_MEMBER(response_time)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(mime_type)
  IPC_STRUCT_TRAITS_MEMBER(charset)
  IPC_STRUCT_TRAITS_MEMBER(is_legacy_symantec_cert)
  IPC_STRUCT_TRAITS_MEMBER(cert_validity_start)
  IPC_STRUCT_TRAITS_MEMBER(content_length)
  IPC_STRUCT_TRAITS_MEMBER(encoded_data_length)
  IPC_STRUCT_TRAITS_MEMBER(encoded_body_length)
  IPC_STRUCT_TRAITS_MEMBER(appcache_id)
  IPC_STRUCT_TRAITS_MEMBER(appcache_manifest_url)
  IPC_STRUCT_TRAITS_MEMBER(load_timing)
  IPC_STRUCT_TRAITS_MEMBER(raw_request_response_info)
  IPC_STRUCT_TRAITS_MEMBER(download_file_path)
  IPC_STRUCT_TRAITS_MEMBER(was_fetched_via_spdy)
  IPC_STRUCT_TRAITS_MEMBER(was_alpn_negotiated)
  IPC_STRUCT_TRAITS_MEMBER(was_alternate_protocol_available)
  IPC_STRUCT_TRAITS_MEMBER(connection_info)
  IPC_STRUCT_TRAITS_MEMBER(alpn_negotiated_protocol)
  IPC_STRUCT_TRAITS_MEMBER(socket_address)
  IPC_STRUCT_TRAITS_MEMBER(was_fetched_via_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(was_fallback_required_by_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(url_list_via_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(response_type_via_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(service_worker_start_time)
  IPC_STRUCT_TRAITS_MEMBER(service_worker_ready_time)
  IPC_STRUCT_TRAITS_MEMBER(is_in_cache_storage)
  IPC_STRUCT_TRAITS_MEMBER(cache_storage_cache_name)
  IPC_STRUCT_TRAITS_MEMBER(did_service_worker_navigation_preload)
  IPC_STRUCT_TRAITS_MEMBER(previews_state)
  IPC_STRUCT_TRAITS_MEMBER(effective_connection_type)
  IPC_STRUCT_TRAITS_MEMBER(certificate)
  IPC_STRUCT_TRAITS_MEMBER(cert_status)
  IPC_STRUCT_TRAITS_MEMBER(ssl_connection_status)
  IPC_STRUCT_TRAITS_MEMBER(ssl_key_exchange_group)
  IPC_STRUCT_TRAITS_MEMBER(signed_certificate_timestamps)
  IPC_STRUCT_TRAITS_MEMBER(cors_exposed_header_names)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(net::MutableNetworkTrafficAnnotationTag)
  IPC_STRUCT_TRAITS_MEMBER(unique_id_hash_code)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ResourceRequest)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(site_for_cookies)
  IPC_STRUCT_TRAITS_MEMBER(request_initiator)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(referrer_policy)
  IPC_STRUCT_TRAITS_MEMBER(is_prerendering)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(load_flags)
  IPC_STRUCT_TRAITS_MEMBER(plugin_child_id)
  IPC_STRUCT_TRAITS_MEMBER(resource_type)
  IPC_STRUCT_TRAITS_MEMBER(priority)
  IPC_STRUCT_TRAITS_MEMBER(request_context)
  IPC_STRUCT_TRAITS_MEMBER(appcache_host_id)
  IPC_STRUCT_TRAITS_MEMBER(should_reset_appcache)
  IPC_STRUCT_TRAITS_MEMBER(service_worker_provider_id)
  IPC_STRUCT_TRAITS_MEMBER(originated_from_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(service_worker_mode)
  IPC_STRUCT_TRAITS_MEMBER(fetch_request_mode)
  IPC_STRUCT_TRAITS_MEMBER(fetch_credentials_mode)
  IPC_STRUCT_TRAITS_MEMBER(fetch_redirect_mode)
  IPC_STRUCT_TRAITS_MEMBER(fetch_integrity)
  IPC_STRUCT_TRAITS_MEMBER(fetch_request_context_type)
  IPC_STRUCT_TRAITS_MEMBER(fetch_frame_type)
  IPC_STRUCT_TRAITS_MEMBER(request_body)
  IPC_STRUCT_TRAITS_MEMBER(download_to_file)
  IPC_STRUCT_TRAITS_MEMBER(keepalive)
  IPC_STRUCT_TRAITS_MEMBER(has_user_gesture)
  IPC_STRUCT_TRAITS_MEMBER(enable_load_timing)
  IPC_STRUCT_TRAITS_MEMBER(enable_upload_progress)
  IPC_STRUCT_TRAITS_MEMBER(do_not_prompt_for_login)
  IPC_STRUCT_TRAITS_MEMBER(render_frame_id)
  IPC_STRUCT_TRAITS_MEMBER(is_main_frame)
  IPC_STRUCT_TRAITS_MEMBER(transition_type)
  IPC_STRUCT_TRAITS_MEMBER(should_replace_current_entry)
  IPC_STRUCT_TRAITS_MEMBER(transferred_request_child_id)
  IPC_STRUCT_TRAITS_MEMBER(transferred_request_request_id)
  IPC_STRUCT_TRAITS_MEMBER(allow_download)
  IPC_STRUCT_TRAITS_MEMBER(report_raw_headers)
  IPC_STRUCT_TRAITS_MEMBER(previews_state)
  IPC_STRUCT_TRAITS_MEMBER(resource_body_stream_url)
  IPC_STRUCT_TRAITS_MEMBER(initiated_in_secure_context)
  IPC_STRUCT_TRAITS_MEMBER(download_to_network_cache_only)
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_COMMON_RESOURCE_MESSAGES_H_
