// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FILEAPI_WEBBLOB_MESSAGES_H_
#define CONTENT_COMMON_FILEAPI_WEBBLOB_MESSAGES_H_

// IPC messages for HTML5 Blob and Stream.

#include <stddef.h>

#include <set>

#include "base/memory/shared_memory.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "services/network/public/cpp/data_element.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "storage/common/blob_storage/blob_storage_constants.h"
#include "url/ipc/url_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START BlobMsgStart

// Trait definitions for async blob transport messages.

IPC_ENUM_TRAITS_MAX_VALUE(storage::IPCBlobItemRequestStrategy,
                          storage::IPCBlobItemRequestStrategy::LAST)
IPC_ENUM_TRAITS_MAX_VALUE(storage::BlobStatus, storage::BlobStatus::LAST)

IPC_STRUCT_TRAITS_BEGIN(storage::BlobItemBytesRequest)
  IPC_STRUCT_TRAITS_MEMBER(request_number)
  IPC_STRUCT_TRAITS_MEMBER(transport_strategy)
  IPC_STRUCT_TRAITS_MEMBER(renderer_item_index)
  IPC_STRUCT_TRAITS_MEMBER(renderer_item_offset)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(handle_index)
  IPC_STRUCT_TRAITS_MEMBER(handle_offset)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(storage::BlobItemBytesResponse)
  IPC_STRUCT_TRAITS_MEMBER(request_number)
  IPC_STRUCT_TRAITS_MEMBER(inline_data)
  IPC_STRUCT_TRAITS_MEMBER(time_file_modified)
IPC_STRUCT_TRAITS_END()

// This message is to tell the browser that we will be building a blob. The
// DataElements are used to:
// * describe & transport non-memory resources (blobs, files, etc)
// * describe the size of memory items
// * 'shortcut' transport the memory up to the IPC limit so the browser can use
//   it if it's not currently full.
// See https://bit.ly/BlobStorageRefactor
//
// NOTE: This message is synchronous to ensure that the browser is aware of the
// UUID before the UUID is passed to another process. This protects against a
// race condition in which the browser could be asked about a UUID that doesn't
// yet exist from its perspective. See also https://goo.gl/bfdE64.
//
IPC_SYNC_MESSAGE_CONTROL4_0(
    BlobStorageMsg_RegisterBlob,
    std::string /* uuid */,
    std::string /* content_type */,
    std::string /* content_disposition */,
    std::vector<network::DataElement> /* item_descriptions */)

IPC_MESSAGE_CONTROL4(
    BlobStorageMsg_RequestMemoryItem,
    std::string /* uuid */,
    std::vector<storage::BlobItemBytesRequest> /* requests */,
    std::vector<base::SharedMemoryHandle> /* memory_handles */,
    std::vector<IPC::PlatformFileForTransit> /* file_handles */)

IPC_MESSAGE_CONTROL2(
    BlobStorageMsg_MemoryItemResponse,
    std::string /* uuid */,
    std::vector<storage::BlobItemBytesResponse> /* responses */)

IPC_MESSAGE_CONTROL2(BlobStorageMsg_SendBlobStatus,
                     std::string /* uuid */,
                     storage::BlobStatus /* code */)

IPC_MESSAGE_CONTROL1(BlobHostMsg_IncrementRefCount,
                     std::string /* uuid */)
IPC_MESSAGE_CONTROL1(BlobHostMsg_DecrementRefCount,
                     std::string /* uuid */)
// NOTE: This message is synchronous to ensure that the browser is aware of the
// UUID before the UUID is passed to another process. This protects against a
// race condition in which the browser could be asked about a UUID that doesn't
// yet exist from its perspective. See also https://goo.gl/bfdE64.
IPC_SYNC_MESSAGE_CONTROL2_0(BlobHostMsg_RegisterPublicURL,
                            GURL,
                            std::string /* uuid */)
IPC_MESSAGE_CONTROL1(BlobHostMsg_RevokePublicURL,
                     GURL)

#endif  // CONTENT_COMMON_FILEAPI_WEBBLOB_MESSAGES_H_