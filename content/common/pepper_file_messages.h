// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include "content/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "webkit/plugins/ppapi/dir_contents.h"
#include "webkit/plugins/ppapi/file_path.h"

// Singly-included section, still not converted
#ifndef CHROME_COMMON_PEPPER_FILE_MESSAGES_H_
#define CHROME_COMMON_PEPPER_FILE_MESSAGES_H_

namespace IPC {

// Also needed for Serializing DirContents, which is just a vector of DirEntry.
template <>
struct ParamTraits<webkit::ppapi::DirEntry> {
  typedef webkit::ppapi::DirEntry param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webkit::ppapi::PepperFilePath> {
  typedef webkit::ppapi::PepperFilePath param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_PEPPER_FILE_MESSAGES_H_

#define IPC_MESSAGE_START PepperFileMsgStart

// Trusted Pepper Filesystem messages from the renderer to the browser.

// Open the file.
IPC_SYNC_MESSAGE_CONTROL2_2(PepperFileMsg_OpenFile,
                            webkit::ppapi::PepperFilePath /* path */,
                            int /* flags */,
                            base::PlatformFileError /* error_code */,
                            IPC::PlatformFileForTransit /* result */)

// Rename the file.
IPC_SYNC_MESSAGE_CONTROL2_1(PepperFileMsg_RenameFile,
                            webkit::ppapi::PepperFilePath /* from_path */,
                            webkit::ppapi::PepperFilePath /* to_path */,
                            base::PlatformFileError /* error_code */)

// Delete the file.
IPC_SYNC_MESSAGE_CONTROL2_1(PepperFileMsg_DeleteFileOrDir,
                            webkit::ppapi::PepperFilePath /* path */,
                            bool /* recursive */,
                            base::PlatformFileError /* error_code */)

// Create the directory.
IPC_SYNC_MESSAGE_CONTROL1_1(PepperFileMsg_CreateDir,
                            webkit::ppapi::PepperFilePath /* path */,
                            base::PlatformFileError /* error_code */)

// Query the file's info.
IPC_SYNC_MESSAGE_CONTROL1_2(PepperFileMsg_QueryFile,
                            webkit::ppapi::PepperFilePath /* path */,
                            base::PlatformFileInfo, /* info */
                            base::PlatformFileError /* error_code */)

// Get the directory's contents.
IPC_SYNC_MESSAGE_CONTROL1_2(PepperFileMsg_GetDirContents,
                            webkit::ppapi::PepperFilePath /* path */,
                            webkit::ppapi::DirContents, /* contents */
                            base::PlatformFileError /* error_code */)

