// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHILD_BROKER_HOST_H_
#define MOJO_EDK_SYSTEM_CHILD_BROKER_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/routed_raw_channel.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

// Responds to requests from ChildBroker. This is used to handle message pipe
// multiplexing and sandbox messages. There is one object of this class
// per child process host object.
// This object will delete itself when it notices that the pipe is broken.
class MOJO_SYSTEM_IMPL_EXPORT ChildBrokerHost
    : public RawChannel::Delegate,
#if defined(OS_WIN)
      NON_EXPORTED_BASE(public base::MessageLoopForIO::IOHandler) {
#else
      public base::MessageLoopForIO::Watcher {
#endif
 public:
  // |child_process| is a handle to the child process. It will be duplicated by
  // this object. |pipe| is a handle to the communication pipe to the child
  // process, which is generated inside mojo::edk::ChildProcessLaunched. It is
  // owned by this class.
  ChildBrokerHost(base::ProcessHandle child_process, ScopedPlatformHandle pipe);

  base::ProcessId GetProcessId();

  // Sends a message to the child process to connect to |process_id| via |pipe|.
  void ConnectToProcess(base::ProcessId process_id, ScopedPlatformHandle pipe);

  // Sends a message to the child process that |pipe_id|'s other end is in
  // |process_id|. If the other end is in this parent process, |process_id| will
  // be 0.
  void ConnectMessagePipe(uint64_t pipe_id, base::ProcessId process_id);

  // Sends a message to the child process informing it that the peer process has
  // died before it could connect.
  void PeerDied(uint64_t pipe_id);

  RoutedRawChannel* channel() { return child_channel_; }

 private:
  ~ChildBrokerHost() override;

  void InitOnIO(ScopedPlatformHandle parent_async_channel_handle);

  // RawChannel::Delegate implementation:
  void OnReadMessage(
      const MessageInTransit::View& message_view,
      ScopedPlatformHandleVectorPtr platform_handles) override;
  void OnError(Error error) override;

  // Callback for when child_channel_ is destroyed.
  void ChannelDestructed(RoutedRawChannel* channel);

#if defined(OS_WIN)
  void BeginRead();

  // base::MessageLoopForIO::IOHandler implementation:
  void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                     DWORD bytes_transferred,
                     DWORD error) override;

  // Helper wrappers around DuplicateHandle.
  HANDLE DuplicateToChild(HANDLE handle);
  HANDLE DuplicateFromChild(HANDLE handle);
#else
  // Reads all currently available data, and responds to any completed messages.
  void TryReadAndWriteHandles();

  // base::MessageLoopForIO::Watcher implementation:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;
#endif

  base::ProcessId process_id_;

  // Channel used to receive and send multiplexing related messages.
  RoutedRawChannel* child_channel_;

  // Pipe used for synchronous messages from the child. Responses are written to
  // it as well.
  ScopedPlatformHandle sync_channel_;

#if defined(OS_WIN)
  // Handle to the child process, used for duplication of handles.
  base::Process child_process_;
  std::vector<char> write_data_;
  base::MessageLoopForIO::IOContext read_context_;
  base::MessageLoopForIO::IOContext write_context_;
#else
  std::deque<PlatformHandle> write_handles_;
  base::MessageLoopForIO::FileDescriptorWatcher fd_controller_;
#endif

  std::vector<char> read_data_;
  // How many bytes in read_data_ we already read.
  uint32_t num_bytes_read_;

  DISALLOW_COPY_AND_ASSIGN(ChildBrokerHost);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CHILD_BROKER_HOST_H_
