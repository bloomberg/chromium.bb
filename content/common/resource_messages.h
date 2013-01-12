// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for resource loading.

// Multiply-included message file, hence no include guard.
#include "base/process.h"
#include "base/shared_memory.h"
#include "content/common/content_param_traits_macros.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/resource_response.h"
#include "ipc/ipc_message_macros.h"
#include "webkit/glue/resource_request_body.h"

#ifndef CONTENT_COMMON_RESOURCE_MESSAGES_H_
#define CONTENT_COMMON_RESOURCE_MESSAGES_H_

namespace webkit_glue {
struct ResourceDevToolsInfo;
struct ResourceLoadTimingInfo;
}

namespace IPC {

template <>
struct ParamTraits<scoped_refptr<net::HttpResponseHeaders> > {
  typedef scoped_refptr<net::HttpResponseHeaders> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<webkit_base::DataElement> {
  typedef webkit_base::DataElement param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<scoped_refptr<webkit_glue::ResourceDevToolsInfo> > {
  typedef scoped_refptr<webkit_glue::ResourceDevToolsInfo> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webkit_glue::ResourceLoadTimingInfo> {
  typedef webkit_glue::ResourceLoadTimingInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<scoped_refptr<webkit_glue::ResourceRequestBody> > {
  typedef scoped_refptr<webkit_glue::ResourceRequestBody> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_RESOURCE_MESSAGES_H_


#define IPC_MESSAGE_START ResourceMsgStart
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_STRUCT_TRAITS_BEGIN(content::ResourceResponseHead)
  IPC_STRUCT_TRAITS_PARENT(webkit_glue::ResourceResponseInfo)
  IPC_STRUCT_TRAITS_MEMBER(error_code)
  IPC_STRUCT_TRAITS_MEMBER(request_start)
  IPC_STRUCT_TRAITS_MEMBER(response_start)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyncLoadResult)
  IPC_STRUCT_TRAITS_PARENT(content::ResourceResponseHead)
  IPC_STRUCT_TRAITS_MEMBER(final_url)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::ResourceResponseInfo)
  IPC_STRUCT_TRAITS_MEMBER(request_time)
  IPC_STRUCT_TRAITS_MEMBER(response_time)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(mime_type)
  IPC_STRUCT_TRAITS_MEMBER(charset)
  IPC_STRUCT_TRAITS_MEMBER(security_info)
  IPC_STRUCT_TRAITS_MEMBER(content_length)
  IPC_STRUCT_TRAITS_MEMBER(encoded_data_length)
  IPC_STRUCT_TRAITS_MEMBER(appcache_id)
  IPC_STRUCT_TRAITS_MEMBER(appcache_manifest_url)
  IPC_STRUCT_TRAITS_MEMBER(connection_id)
  IPC_STRUCT_TRAITS_MEMBER(connection_reused)
  IPC_STRUCT_TRAITS_MEMBER(load_timing)
  IPC_STRUCT_TRAITS_MEMBER(devtools_info)
  IPC_STRUCT_TRAITS_MEMBER(download_file_path)
  IPC_STRUCT_TRAITS_MEMBER(was_fetched_via_spdy)
  IPC_STRUCT_TRAITS_MEMBER(was_npn_negotiated)
  IPC_STRUCT_TRAITS_MEMBER(was_alternate_protocol_available)
  IPC_STRUCT_TRAITS_MEMBER(was_fetched_via_proxy)
  IPC_STRUCT_TRAITS_MEMBER(npn_negotiated_protocol)
  IPC_STRUCT_TRAITS_MEMBER(socket_address)
IPC_STRUCT_TRAITS_END()

// Parameters for a resource request.
IPC_STRUCT_BEGIN(ResourceHostMsg_Request)
  // The request method: GET, POST, etc.
  IPC_STRUCT_MEMBER(std::string, method)

  // The requested URL.
  IPC_STRUCT_MEMBER(GURL, url)

  // Usually the URL of the document in the top-level window, which may be
  // checked by the third-party cookie blocking policy. Leaving it empty may
  // lead to undesired cookie blocking. Third-party cookie blocking can be
  // bypassed by setting first_party_for_cookies = url, but this should ideally
  // only be done if there really is no way to determine the correct value.
  IPC_STRUCT_MEMBER(GURL, first_party_for_cookies)

  // The referrer to use (may be empty).
  IPC_STRUCT_MEMBER(GURL, referrer)

  // The referrer policy to use.
  IPC_STRUCT_MEMBER(WebKit::WebReferrerPolicy, referrer_policy)

  // Additional HTTP request headers.
  IPC_STRUCT_MEMBER(std::string, headers)

  // net::URLRequest load flags (0 by default).
  IPC_STRUCT_MEMBER(int, load_flags)

  // Process ID from which this request originated, or zero if it originated
  // in the renderer itself.
  IPC_STRUCT_MEMBER(int, origin_pid)

  // What this resource load is for (main frame, sub-frame, sub-resource,
  // object).
  IPC_STRUCT_MEMBER(ResourceType::Type, resource_type)

  // Used by plugin->browser requests to get the correct net::URLRequestContext.
  IPC_STRUCT_MEMBER(uint32, request_context)

  // Indicates which frame (or worker context) the request is being loaded into,
  // or kNoHostId.
  IPC_STRUCT_MEMBER(int, appcache_host_id)

  // Optional resource request body (may be null).
  IPC_STRUCT_MEMBER(scoped_refptr<webkit_glue::ResourceRequestBody>,
                    request_body)

  IPC_STRUCT_MEMBER(bool, download_to_file)

  // True if the request was user initiated.
  IPC_STRUCT_MEMBER(bool, has_user_gesture)

  // True if |frame_id| is the main frame of a RenderView.
  IPC_STRUCT_MEMBER(bool, is_main_frame)

  // Identifies the frame within the RenderView that sent the request.
  // -1 if unknown / invalid.
  IPC_STRUCT_MEMBER(int64, frame_id)

  // True if |parent_frame_id| is the main frame of a RenderView.
  IPC_STRUCT_MEMBER(bool, parent_is_main_frame)

  // Identifies the parent frame of the frame that sent the request.
  // -1 if unknown / invalid.
  IPC_STRUCT_MEMBER(int64, parent_frame_id)

  IPC_STRUCT_MEMBER(content::PageTransition, transition_type)

  // The following two members identify a previous request that has been
  // created before this navigation has been transferred to a new render view.
  // This serves the purpose of recycling the old request.
  // Unless this refers to a transferred navigation, these values are -1 and -1.
  IPC_STRUCT_MEMBER(int, transferred_request_child_id)
  IPC_STRUCT_MEMBER(int, transferred_request_request_id)

  // Whether or not we should allow the URL to download.
  IPC_STRUCT_MEMBER(bool, allow_download)
IPC_STRUCT_END()

// Resource messages sent from the browser to the renderer.

// Sent when the headers are available for a resource request.
IPC_MESSAGE_ROUTED2(ResourceMsg_ReceivedResponse,
                    int /* request_id */,
                    content::ResourceResponseHead)

// Sent when cached metadata from a resource request is ready.
IPC_MESSAGE_ROUTED2(ResourceMsg_ReceivedCachedMetadata,
                    int /* request_id */,
                    std::vector<char> /* data */)

// Sent as upload progress is being made.
IPC_MESSAGE_ROUTED3(ResourceMsg_UploadProgress,
                    int /* request_id */,
                    int64 /* position */,
                    int64 /* size */)

// Sent when the request has been redirected.  The receiver is expected to
// respond with either a FollowRedirect message (if the redirect is to be
// followed) or a CancelRequest message (if it should not be followed).
IPC_MESSAGE_ROUTED3(ResourceMsg_ReceivedRedirect,
                    int /* request_id */,
                    GURL /* new_url */,
                    content::ResourceResponseHead)

// Sent to set the shared memory buffer to be used to transmit response data to
// the renderer.  Subsequent DataReceived messages refer to byte ranges in the
// shared memory buffer.  The shared memory buffer should be retained by the
// renderer until the resource request completes.
//
// NOTE: The shared memory handle should already be mapped into the process
// that receives this message.
//
// TODO(darin): The |renderer_pid| parameter is just a temporary parameter,
// added to help in debugging crbug/160401.
//
IPC_MESSAGE_ROUTED4(ResourceMsg_SetDataBuffer,
                    int /* request_id */,
                    base::SharedMemoryHandle /* shm_handle */,
                    int /* shm_size */,
                    base::ProcessId /* renderer_pid */)

// Sent when some data from a resource request is ready.  The data offset and
// length specify a byte range into the shared memory buffer provided by the
// SetDataBuffer message.
IPC_MESSAGE_ROUTED4(ResourceMsg_DataReceived,
                    int /* request_id */,
                    int /* data_offset */,
                    int /* data_length */,
                    int /* encoded_data_length */)

// Sent when some data from a resource request has been downloaded to
// file. This is only called in the 'download_to_file' case and replaces
// ResourceMsg_DataReceived in the call sequence in that case.
IPC_MESSAGE_ROUTED2(ResourceMsg_DataDownloaded,
                    int /* request_id */,
                    int /* data_len */)

// Sent when the request has been completed.
IPC_MESSAGE_ROUTED5(ResourceMsg_RequestComplete,
                    int /* request_id */,
                    int /* error_code */,
                    bool /* was_ignored_by_handler */,
                    std::string /* security info */,
                    base::TimeTicks /* completion_time */)

// Resource messages sent from the renderer to the browser.

// Makes a resource request via the browser.
IPC_MESSAGE_ROUTED2(ResourceHostMsg_RequestResource,
                    int /* request_id */,
                    ResourceHostMsg_Request)

// Cancels a resource request with the ID given as the parameter.
IPC_MESSAGE_ROUTED1(ResourceHostMsg_CancelRequest,
                    int /* request_id */)

// Follows a redirect that occured for the resource request with the ID given
// as the parameter.
IPC_MESSAGE_ROUTED3(ResourceHostMsg_FollowRedirect,
                    int /* request_id */,
                    bool /* has_new_first_party_for_cookies */,
                    GURL /* new_first_party_for_cookies */)

// Makes a synchronous resource request via the browser.
IPC_SYNC_MESSAGE_ROUTED2_1(ResourceHostMsg_SyncLoad,
                           int /* request_id */,
                           ResourceHostMsg_Request,
                           content::SyncLoadResult)

// Sent when the renderer process is done processing a DataReceived
// message.
IPC_MESSAGE_ROUTED1(ResourceHostMsg_DataReceived_ACK,
                    int /* request_id */)

// Sent when the renderer has processed a DataDownloaded message.
IPC_MESSAGE_ROUTED1(ResourceHostMsg_DataDownloaded_ACK,
                    int /* request_id */)

// Sent by the renderer process to acknowledge receipt of a
// UploadProgress message.
IPC_MESSAGE_ROUTED1(ResourceHostMsg_UploadProgress_ACK,
                    int /* request_id */)

// Sent when the renderer process deletes a resource loader.
IPC_MESSAGE_CONTROL1(ResourceHostMsg_ReleaseDownloadedFile,
                     int /* request_id */)
