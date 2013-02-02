// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_PNACL_FILE_HOST_H_
#define CHROME_BROWSER_NACL_HOST_PNACL_FILE_HOST_H_

#include <string>

class ChromeRenderMessageFilter;

namespace base {
class FilePath;
}

namespace IPC {
class Message;
}  // namespace IPC

// Opens Pnacl Files in the Browser process, on behalf of the NaCl plugin.

namespace pnacl_file_host {

// Open a Pnacl file (readonly) on behalf of the NaCl plugin.
void GetReadonlyPnaclFd(ChromeRenderMessageFilter* chrome_render_message_filter,
                        const std::string& filename,
                        IPC::Message* reply_msg);

// Return true if the filename requested is valid for opening.
// Sets file_to_open to the base::FilePath which we will attempt to open.
bool PnaclCanOpenFile(const std::string& filename,
                      base::FilePath* file_to_open);

// Creates a temporary file that will be deleted when the last handle
// is closed, or earlier.
void CreateTemporaryFile(
    ChromeRenderMessageFilter* chrome_render_message_filter,
    IPC::Message* reply_msg);

}  // namespace pnacl_file_host

#endif  // CHROME_BROWSER_NACL_HOST_PNACL_FILE_HOST_H_
