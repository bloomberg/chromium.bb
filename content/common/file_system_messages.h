// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the file system.
// Multiply-included message file, hence no include guard.

#include "base/file_util_proxy.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "webkit/fileapi/file_system_types.h"

#define IPC_MESSAGE_START FileSystemMsgStart

IPC_STRUCT_TRAITS_BEGIN(base::FileUtilProxy::Entry)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(is_directory)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS(fileapi::FileSystemType)

// File system messages sent from the browser to the child process.

// WebFrameClient::openFileSystem response messages.
IPC_MESSAGE_CONTROL4(FileSystemMsg_OpenComplete,
                     int /* request_id */,
                     bool /* accepted */,
                     std::string /* name */,
                     GURL /* root_url */)

// WebFileSystem response messages.
IPC_MESSAGE_CONTROL1(FileSystemMsg_DidSucceed,
                     int /* request_id */)
IPC_MESSAGE_CONTROL3(FileSystemMsg_DidReadMetadata,
                     int /* request_id */,
                     base::PlatformFileInfo,
                     FilePath /* true platform path, where possible */)
IPC_MESSAGE_CONTROL3(FileSystemMsg_DidReadDirectory,
                     int /* request_id */,
                     std::vector<base::FileUtilProxy::Entry> /* entries */,
                     bool /* has_more */)
IPC_MESSAGE_CONTROL3(FileSystemMsg_DidWrite,
                     int /* request_id */,
                     int64 /* byte count */,
                     bool /* complete */)
IPC_MESSAGE_CONTROL2(FileSystemMsg_DidOpenFile,
                     int /* request_id */,
                     IPC::PlatformFileForTransit)
IPC_MESSAGE_CONTROL2(FileSystemMsg_DidFail,
                     int /* request_id */,
                     base::PlatformFileError /* error_code */)

// File system messages sent from the child process to the browser.

// WebFrameClient::openFileSystem() message.
IPC_MESSAGE_CONTROL5(FileSystemHostMsg_Open,
                     int /* request_id */,
                     GURL /* origin_url */,
                     fileapi::FileSystemType /* type */,
                     int64 /* requested_size */,
                     bool /* create */)

// WebFileSystem::move() message.
IPC_MESSAGE_CONTROL3(FileSystemHostMsg_Move,
                     int /* request_id */,
                     GURL /* src path */,
                     GURL /* dest path */)

// WebFileSystem::copy() message.
IPC_MESSAGE_CONTROL3(FileSystemHostMsg_Copy,
                     int /* request_id */,
                     GURL /* src path */,
                     GURL /* dest path */)

// WebFileSystem::remove() message.
IPC_MESSAGE_CONTROL3(FileSystemMsg_Remove,
                     int /* request_id */,
                     GURL /* path */,
                     bool /* recursive */)

// WebFileSystem::readMetadata() message.
IPC_MESSAGE_CONTROL2(FileSystemHostMsg_ReadMetadata,
                     int /* request_id */,
                     GURL /* path */)

// WebFileSystem::create() message.
IPC_MESSAGE_CONTROL5(FileSystemHostMsg_Create,
                     int /* request_id */,
                     GURL /* path */,
                     bool /* exclusive */,
                     bool /* is_directory */,
                     bool /* recursive */)

// WebFileSystem::exists() messages.
IPC_MESSAGE_CONTROL3(FileSystemHostMsg_Exists,
                     int /* request_id */,
                     GURL /* path */,
                     bool /* is_directory */)

// WebFileSystem::readDirectory() message.
IPC_MESSAGE_CONTROL2(FileSystemHostMsg_ReadDirectory,
                     int /* request_id */,
                     GURL /* path */)

// WebFileWriter::write() message.
IPC_MESSAGE_CONTROL4(FileSystemHostMsg_Write,
                     int /* request id */,
                     GURL /* file path */,
                     GURL /* blob URL */,
                     int64 /* position */)

// WebFileWriter::truncate() message.
IPC_MESSAGE_CONTROL3(FileSystemHostMsg_Truncate,
                     int /* request id */,
                     GURL /* file path */,
                     int64 /* length */)

// Pepper's Touch() message.
IPC_MESSAGE_CONTROL4(FileSystemHostMsg_TouchFile,
                     int /* request_id */,
                     GURL /* path */,
                     base::Time /* last_access_time */,
                     base::Time /* last_modified_time */)

// WebFileWriter::cancel() message.
IPC_MESSAGE_CONTROL2(FileSystemHostMsg_CancelWrite,
                     int /* request id */,
                     int /* id of request to cancel */)

// Pepper's OpenFile message.
IPC_MESSAGE_CONTROL3(FileSystemHostMsg_OpenFile,
                     int /* request id */,
                     GURL /* file path */,
                     int /* file flags */)

// For Pepper's URL loader.
IPC_SYNC_MESSAGE_CONTROL1_1(FileSystemHostMsg_SyncGetPlatformPath,
                            GURL /* file path */,
                            FilePath /* platform_path */)

// Pre- and post-update notifications for ppapi implementation.
IPC_MESSAGE_CONTROL1(FileSystemHostMsg_WillUpdate,
                     GURL /* file_path */)

IPC_MESSAGE_CONTROL2(FileSystemHostMsg_DidUpdate,
                     GURL /* file_path */,
                     int64 /* delta */)
