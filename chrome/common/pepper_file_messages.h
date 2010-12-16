// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PEPPER_FILE_MESSAGES_H_
#define CHROME_COMMON_PEPPER_FILE_MESSAGES_H_
#pragma once

#include "chrome/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "webkit/plugins/ppapi/dir_contents.h"

#define IPC_MESSAGE_START PepperFileMsgStart

namespace IPC {

// Also needed for Serializing DirContents, which is just a vector of DirEntry.
template <>
struct ParamTraits<webkit::ppapi::DirEntry> {
  typedef webkit::ppapi::DirEntry param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

// Trusted Pepper Filesystem messages from the renderer to the browser.

// Open the file.
IPC_SYNC_MESSAGE_CONTROL2_2(PepperFileMsg_OpenFile,
                            FilePath /* path */,
                            int /* flags */,
                            base::PlatformFileError /* error_code */,
                            IPC::PlatformFileForTransit /* result */)

// Rename the file.
IPC_SYNC_MESSAGE_CONTROL2_1(PepperFileMsg_RenameFile,
                            FilePath /* path_from */,
                            FilePath /* path_to */,
                            base::PlatformFileError /* error_code */)

// Delete the file.
IPC_SYNC_MESSAGE_CONTROL2_1(PepperFileMsg_DeleteFileOrDir,
                            FilePath /* path */,
                            bool /* recursive */,
                            base::PlatformFileError /* error_code */)

// Create the directory.
IPC_SYNC_MESSAGE_CONTROL1_1(PepperFileMsg_CreateDir,
                            FilePath /* path */,
                            base::PlatformFileError /* error_code */)

// Query the file's info.
IPC_SYNC_MESSAGE_CONTROL1_2(PepperFileMsg_QueryFile,
                            FilePath /* path */,
                            base::PlatformFileInfo, /* info */
                            base::PlatformFileError /* error_code */)

// Get the directory's contents.
IPC_SYNC_MESSAGE_CONTROL1_2(PepperFileMsg_GetDirContents,
                            FilePath /* path */,
                            webkit::ppapi::DirContents, /* contents */
                            base::PlatformFileError /* error_code */)

#endif  // CHROME_COMMON_PEPPER_FILE_MESSAGES_H_
