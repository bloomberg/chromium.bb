// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "components/arc/standalone/arc_standalone_bridge_runner.h"
#include "mojo/edk/embedder/embedder.h"

int g_pipe_fd[2];

class TerminationReceiver : public base::MessageLoopForIO::Watcher {
 public:
  explicit TerminationReceiver(arc::ArcStandaloneBridgeRunner* runner)
      : runner_(runner) {
  }

  void OnFileCanReadWithoutBlocking(int fd) override { runner_->Stop(1); }
  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

 private:
  arc::ArcStandaloneBridgeRunner* runner_;

  DISALLOW_COPY_AND_ASSIGN(TerminationReceiver);
};

void TerminationHandler(int /* signum */) {
  int ret = HANDLE_EINTR(write(g_pipe_fd[1], "1", 1));
  if (ret < 0) {
    LOG(ERROR) << "Failed to write pipe fd:" << ret;
    _exit(2);  // We still need to exit the program, but in a brute force way.
  }
}

void RegisterSignalHandlers() {
  struct sigaction action = {};
  CHECK_EQ(0, sigemptyset(&action.sa_mask));
  action.sa_handler = TerminationHandler;
  action.sa_flags = 0;

  for (int sig : {SIGINT, SIGTERM}) {
    CHECK_EQ(0, sigaction(sig, &action, NULL));
  }
}

void CleanUp() {
  // Best effort clean up.
  struct sigaction action = {};
  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_DFL;
  action.sa_flags = 0;
  for (int sig : {SIGINT, SIGTERM}) {
    sigaction(sig, &action, NULL);
  }

  close(g_pipe_fd[1]);
  close(g_pipe_fd[0]);
}

// This is experimental only. Do not use in production.
// All threads in the standalone runner must be I/O threads.
int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager at_exit_manager;
  mojo::edk::Init();

  // Initialize pipe
  CHECK_EQ(0, pipe(g_pipe_fd));
  fcntl(g_pipe_fd[1], F_SETFL, O_NONBLOCK);

  // Instantiate runner, which instantiates message loop.
  arc::ArcStandaloneBridgeRunner runner;

  // Set up signal handlers.
  // If signal handler failed to write to pipe, it will just exit the
  // program anyway.
  TerminationReceiver receiver(&runner);
  base::MessageLoopForIO::FileDescriptorWatcher watcher;
  CHECK(base::MessageLoopForIO::current()->WatchFileDescriptor(
      g_pipe_fd[0],
      true, /* persistent */
      base::MessageLoopForIO::WATCH_READ,
      &watcher,
      &receiver));
  RegisterSignalHandlers();

  // Spin the message loop.
  int exit_code = runner.Run();
  CleanUp();
  return exit_code;
}
