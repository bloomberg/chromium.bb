// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

#ifndef CONTENT_BROWSER_RENDERER_HOST_SANDBOX_IPC_H_
#define CONTENT_BROWSER_RENDERER_HOST_SANDBOX_IPC_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "content/child/blink_platform_impl.h"
#include "skia/ext/skia_utils_base.h"

namespace content {

class SandboxIPCProcess {
 public:
  // lifeline_fd: this is the read end of a pipe which the browser process
  //   holds the other end of. If the browser process dies, its descriptors are
  //   closed and we will noticed an EOF on the pipe. That's our signal to exit.
  // browser_socket: the browser's end of the sandbox IPC socketpair. From the
  //   point of view of the renderer, it's talking to the browser but this
  //   object actually services the requests.
  // sandbox_cmd: the path of the sandbox executable.
  SandboxIPCProcess(int lifeline_fd,
                    int browser_socket,
                    std::string sandbox_cmd);
  ~SandboxIPCProcess();

  void Run();

 private:
  void EnsureWebKitInitialized();

  int FindOrAddPath(const SkString& path);

  void HandleRequestFromRenderer(int fd);

  void HandleFontMatchRequest(int fd,
                              const Pickle& pickle,
                              PickleIterator iter,
                              const std::vector<base::ScopedFD*>& fds);

  void HandleFontOpenRequest(int fd,
                             const Pickle& pickle,
                             PickleIterator iter,
                             const std::vector<base::ScopedFD*>& fds);

  void HandleGetFontFamilyForChar(int fd,
                                  const Pickle& pickle,
                                  PickleIterator iter,
                                  const std::vector<base::ScopedFD*>& fds);

  void HandleGetStyleForStrike(int fd,
                               const Pickle& pickle,
                               PickleIterator iter,
                               const std::vector<base::ScopedFD*>& fds);

  void HandleLocaltime(int fd,
                       const Pickle& pickle,
                       PickleIterator iter,
                       const std::vector<base::ScopedFD*>& fds);

  void HandleGetChildWithInode(int fd,
                               const Pickle& pickle,
                               PickleIterator iter,
                               const std::vector<base::ScopedFD*>& fds);

  void HandleMakeSharedMemorySegment(int fd,
                                     const Pickle& pickle,
                                     PickleIterator iter,
                                     const std::vector<base::ScopedFD*>& fds);

  void HandleMatchWithFallback(int fd,
                               const Pickle& pickle,
                               PickleIterator iter,
                               const std::vector<base::ScopedFD*>& fds);

  void SendRendererReply(const std::vector<base::ScopedFD*>& fds,
                         const Pickle& reply,
                         int reply_fd);

  const int lifeline_fd_;
  const int browser_socket_;
  std::vector<std::string> sandbox_cmd_;
  scoped_ptr<BlinkPlatformImpl> webkit_platform_support_;
  SkTDArray<SkString*> paths_;

  DISALLOW_COPY_AND_ASSIGN(SandboxIPCProcess);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SANDBOX_IPC_H_
