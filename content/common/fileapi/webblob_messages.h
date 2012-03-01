// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for HTML5 Blob.
// Multiply-included message file, hence no include guard.

#include "content/public/common/common_param_traits.h"
#include "content/public/common/webkit_param_traits.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START BlobMsgStart

IPC_ENUM_TRAITS(webkit_blob::BlobData::Type)

IPC_STRUCT_TRAITS_BEGIN(webkit_blob::BlobData::Item)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(file_path)
  IPC_STRUCT_TRAITS_MEMBER(blob_url)
  IPC_STRUCT_TRAITS_MEMBER(offset)
  IPC_STRUCT_TRAITS_MEMBER(length)
  IPC_STRUCT_TRAITS_MEMBER(expected_modification_time)
IPC_STRUCT_TRAITS_END()

// Blob messages sent from the renderer to the browser.


// Registers a blob as being built.
IPC_MESSAGE_CONTROL1(BlobHostMsg_StartBuildingBlob,
                     GURL /* url */)

// Appends data to a blob being built.
IPC_MESSAGE_CONTROL2(BlobHostMsg_AppendBlobDataItem,
                     GURL /* url */,
                     webkit_blob::BlobData::Item)

// Appends data to a blob being built.
IPC_SYNC_MESSAGE_CONTROL3_0(BlobHostMsg_SyncAppendSharedMemory,
                            GURL /* url */,
                            base::SharedMemoryHandle,
                            size_t /* buffer size */)

// Finishes building a blob.
IPC_MESSAGE_CONTROL2(BlobHostMsg_FinishBuildingBlob,
                     GURL /* url */,
                     std::string /* content_type */)

// Creates a new blob that's a clone of an existing src blob.
// The source blob must be fully built.
IPC_MESSAGE_CONTROL2(BlobHostMsg_CloneBlob,
                     GURL /* url */,
                     GURL /* src_url */)

// Removes a blob.
IPC_MESSAGE_CONTROL1(BlobHostMsg_RemoveBlob,
                     GURL /* url */)
