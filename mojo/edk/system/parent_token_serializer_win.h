// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PARENT_TOKEN_SERIALIZER_WIN_H_
#define MOJO_EDK_SYSTEM_PARENT_TOKEN_SERIALIZER_WIN_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

// Responds to requests from a child process to exchange handles to tokens and
// vice versa. There is one object of this class per child process host object.
// This object will delete itself when it notices that the pipe is broken.
class MOJO_SYSTEM_IMPL_EXPORT ParentTokenSerializer
    : NON_EXPORTED_BASE(public base::MessageLoopForIO::IOHandler) {
 public:
  // |child_process| is a handle to the child process. It's not owned by this
  // class but is guaranteed to be alive as long as the child process is
  // running. |pipe| is a handle to the communication pipe to the child process,
  // which is generated inside mojo::edk::ChildProcessLaunched. It is owned by
  // this class.
  ParentTokenSerializer(HANDLE child_process, ScopedPlatformHandle pipe);

 private:
  ~ParentTokenSerializer() override;

  void RegisterIOHandler();
  void BeginRead();

  void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                     DWORD bytes_transferred,
                     DWORD error) override;

  // Helper wrappers around DuplicateHandle.
  HANDLE DuplicateToChild(HANDLE handle);
  HANDLE DuplicateFromChild(HANDLE handle);

  HANDLE child_process_;
  ScopedPlatformHandle pipe_;
  base::MessageLoopForIO::IOContext read_context_;
  base::MessageLoopForIO::IOContext write_context_;

  std::vector<char> read_data_;
  // How many bytes in read_data_ we already read.
  uint32_t num_bytes_read_;
  std::vector<char> write_data_;
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PARENT_TOKEN_SERIALIZER_WIN_H_
