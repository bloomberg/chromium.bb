// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_APPCACHE_MESSAGES_H_
#define CONTENT_COMMON_APPCACHE_MESSAGES_H_

#include "ipc/ipc_message_macros.h"

#include <stdint.h>

#include "content/common/appcache_interfaces.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START AppCacheMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::AppCacheEventID,
                          content::AppCacheEventID::APPCACHE_OBSOLETE_EVENT)
IPC_ENUM_TRAITS_MAX_VALUE(content::AppCacheStatus,
                          content::AppCacheStatus::APPCACHE_STATUS_OBSOLETE)
IPC_ENUM_TRAITS_MAX_VALUE(content::AppCacheErrorReason,
                          content::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR)

IPC_STRUCT_TRAITS_BEGIN(content::AppCacheInfo)
  IPC_STRUCT_TRAITS_MEMBER(manifest_url)
  IPC_STRUCT_TRAITS_MEMBER(creation_time)
  IPC_STRUCT_TRAITS_MEMBER(last_update_time)
  IPC_STRUCT_TRAITS_MEMBER(last_access_time)
  IPC_STRUCT_TRAITS_MEMBER(cache_id)
  IPC_STRUCT_TRAITS_MEMBER(group_id)
  IPC_STRUCT_TRAITS_MEMBER(status)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(is_complete)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AppCacheResourceInfo)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(is_master)
  IPC_STRUCT_TRAITS_MEMBER(is_manifest)
  IPC_STRUCT_TRAITS_MEMBER(is_fallback)
  IPC_STRUCT_TRAITS_MEMBER(is_foreign)
  IPC_STRUCT_TRAITS_MEMBER(is_explicit)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AppCacheErrorDetails)
IPC_STRUCT_TRAITS_MEMBER(message)
IPC_STRUCT_TRAITS_MEMBER(reason)
IPC_STRUCT_TRAITS_MEMBER(url)
IPC_STRUCT_TRAITS_MEMBER(status)
IPC_STRUCT_TRAITS_MEMBER(is_cross_origin)
IPC_STRUCT_TRAITS_END()


// AppCache messages sent from the browser to the child process.

// Notifies the renderer of the appcache that has been selected for a
// a particular host. This is sent in reply to AppCacheHostMsg_SelectCache.
IPC_MESSAGE_CONTROL2(AppCacheMsg_CacheSelected,
                     int /* host_id */,
                     content::AppCacheInfo)

// Notifies the renderer of an AppCache status change.
IPC_MESSAGE_CONTROL2(AppCacheMsg_StatusChanged,
                     std::vector<int> /* host_ids */,
                     content::AppCacheStatus)

// Notifies the renderer of an AppCache event other than the
// progress event which has a seperate message.
IPC_MESSAGE_CONTROL2(AppCacheMsg_EventRaised,
                     std::vector<int> /* host_ids */,
                     content::AppCacheEventID)

// Notifies the renderer of an AppCache progress event.
IPC_MESSAGE_CONTROL4(AppCacheMsg_ProgressEventRaised,
                     std::vector<int> /* host_ids */,
                     GURL /* url being processed */,
                     int /* total */,
                     int /* complete */)

// Notifies the renderer of an AppCache error event.
IPC_MESSAGE_CONTROL2(AppCacheMsg_ErrorEventRaised,
                     std::vector<int> /* host_ids */,
                     content::AppCacheErrorDetails)

// Notifies the renderer of an AppCache logging message.
IPC_MESSAGE_CONTROL3(AppCacheMsg_LogMessage,
                     int /* host_id */,
                     int /* log_level */,
                     std::string /* message */)

// Notifies the renderer of the fact that AppCache access was blocked.
IPC_MESSAGE_CONTROL2(AppCacheMsg_ContentBlocked,
                     int /* host_id */,
                     GURL /* manifest_url */)

// In the network service world this message sets the URLLoaderFactory to be
// used for subresources.
IPC_MESSAGE_CONTROL2(AppCacheMsg_SetSubresourceFactory,
                     int /* host_id */,
                     mojo::MessagePipeHandle /* url_loader_factory */)

#endif  // CONTENT_COMMON_APPCACHE_MESSAGES_H_
