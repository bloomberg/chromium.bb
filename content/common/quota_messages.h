// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_QUOTA_MESSAGES_H_
#define CONTENT_COMMON_QUOTA_MESSAGES_H_

#include <stdint.h>

#include "content/public/common/storage_quota_params.h"
#include "ipc/ipc_message_macros.h"
#include "storage/common/quota/quota_types.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START QuotaMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(storage::StorageType, storage::kStorageTypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(storage::QuotaStatusCode, storage::kQuotaStatusLast)

IPC_STRUCT_TRAITS_BEGIN(content::StorageQuotaParams)
  IPC_STRUCT_TRAITS_MEMBER(render_frame_id)
  IPC_STRUCT_TRAITS_MEMBER(request_id)
  IPC_STRUCT_TRAITS_MEMBER(origin_url)
  IPC_STRUCT_TRAITS_MEMBER(storage_type)
  IPC_STRUCT_TRAITS_MEMBER(requested_size)
  IPC_STRUCT_TRAITS_MEMBER(user_gesture)
IPC_STRUCT_TRAITS_END()

// Quota messages sent from the browser to the child process.

IPC_MESSAGE_CONTROL3(QuotaMsg_DidGrantStorageQuota,
                     int /* request_id */,
                     int64_t /* current_usage */,
                     int64_t /* granted_quota */)

IPC_MESSAGE_CONTROL3(QuotaMsg_DidQueryStorageUsageAndQuota,
                     int /* request_id */,
                     int64_t /* current_usage */,
                     int64_t /* current_quota */)

IPC_MESSAGE_CONTROL2(QuotaMsg_DidFail,
                     int /* request_id */,
                     storage::QuotaStatusCode /* error */)

// Quota messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL3(QuotaHostMsg_QueryStorageUsageAndQuota,
                     int /* request_id */,
                     GURL /* origin_url */,
                     storage::StorageType /* type */)

IPC_MESSAGE_CONTROL1(QuotaHostMsg_RequestStorageQuota,
                     content::StorageQuotaParams)

#endif  // CONTENT_COMMON_QUOTA_MESSAGES_H_
